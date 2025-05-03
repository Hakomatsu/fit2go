#include <M5Stack.h>
#include "config.hpp"
#include "TrackerData.hpp"
#include "Storage.hpp"
#include "PulseCounter.hpp"
#include "MetricsCalculator.hpp"
#include "Display.hpp"
#include "WifiManager.hpp"
#include "DataPublisher.hpp"
#include "APConfigPortal.hpp" // APConfigPortal ヘッダー
#include "esp_sleep.h"
#include "esp_err.h"
#include "driver/pcnt.h" // デバッグログ用
#include <time.h>        // ★ NTP関連で追加 ★
#include <sys/time.h>    // ★ gettimeofday で追加 ★


// --- Global Objects ---
Storage storage;
PulseCounter pulseCounter(PULSE_INPUT_PIN);
MetricsCalculator metrics(pulseCounter, storage);
Display display;
WifiManager wifi(storage);
DataPublisher publisher(wifi);
APConfigPortal apPortal(storage, wifi); // APConfigPortal オブジェクト生成

// --- Global State ---
AppState currentState = AppState::INITIALIZING;
bool sessionActive = false;
unsigned long lastDebugPrintTime = 0;
bool timeSynchronized = false; // ★ NTP同期済みフラグを追加 ★
unsigned long lastNtpSyncAttempt = 0; // ★ 前回のNTP同期試行時刻 ★
const unsigned long NTP_SYNC_INTERVAL_MS = 60 * 60 * 1000; // 例: 1時間ごとに再同期試行

// --- Deep Sleep Wakeup Stub ---
void IRAM_ATTR pulseWakeupISR() {}

// --- Deep Sleep Function ---
void goToDeepSleep() {
    Serial.println("Entering deep sleep mode (using esp_deep_sleep_start)...");
    display.showMessage("Sleeping...", 1, true);
    delay(100);
    Serial.println("Appending history data before sleep...");
    if (!storage.appendHistoryDataToSD(metrics.getData())) {
        Serial.println("Failed to append history data!");
    }
    delay(100); // 書き込み待機
    M5.Lcd.sleep(); // LCDをスリープ

    // --- Wakeup Source Configuration ---
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    Serial.printf("Configuring GPIO %d for LOW level wakeup.\n", PULSE_INPUT_PIN);
    esp_err_t err = esp_sleep_enable_ext0_wakeup((gpio_num_t)PULSE_INPUT_PIN, 0);
    if(err != ESP_OK){ Serial.printf("Failed to enable EXT0 wakeup: %s\n", esp_err_to_name(err)); }
    else { Serial.println("EXT0 Wakeup Enabled on LOW level."); }
    Serial.flush();
    esp_deep_sleep_start();
}


// ★★★ NTP同期を開始する関数 ★★★
void initNtp() {
    // まだ同期していない、または前回の試行から一定時間経過した場合のみ実行
    if (!timeSynchronized || millis() - lastNtpSyncAttempt > NTP_SYNC_INTERVAL_MS) {
         Serial.println("Configuring time using NTP...");
         // configTime(GMTオフセット秒, 夏時間オフセット秒, NTPサーバー1, NTPサーバー2)
         configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2);
         lastNtpSyncAttempt = millis();

         // 同期を確認 (少し待つ)
         struct tm timeinfo;
         // getLocalTime は configTime で設定されたタイムゾーン情報を考慮する
         if(!getLocalTime(&timeinfo, 5000)){ // 5秒待っても同期できなければ失敗
             Serial.println("Failed to obtain time from NTP");
             timeSynchronized = false; // 同期失敗
         } else {
             Serial.println("Time synchronized via NTP");
             Serial.printf("Current time (JST): %s", asctime(&timeinfo)); // asctime はローカルタイム文字列を生成
             timeSynchronized = true; // 同期成功
         }
    }
}

// ★★★ 現在のタイムスタンプ(ms)を取得する関数 ★★★
uint64_t getCurrentTimestampMs() {
    if (wifi.isConnected() && timeSynchronized) {
        struct timeval tv;
        gettimeofday(&tv, NULL); // UTCエポックからの秒とマイクロ秒を取得
        // ミリ秒に変換
        uint64_t time_ms = (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
        return time_ms;
    } else {
        // Wi-Fi未接続またはNTP未同期の場合は millis() を返す
        return millis();
    }
}


// --- Arduino Setup ---
void setup() {
    M5.begin(true, true, true, false); // LCD, SD, Serial, I2C=false
    Serial.begin(115200);
    Serial.println("\n\n=== Fitness Tracker Booting ===");

    display.begin();
    display.showMessage("Initializing...", 2, true);

    // Storage初期化 (JSONロード含む)
    if (!storage.begin()) {
        // SDカードが無くても動作は継続するかもしれないが、警告表示
        display.showMessage("SD Card FAIL!", 2);
        Serial.println("WARNING: SD Card initialization failed. Config/Data saving will fail.");
        delay(2000); // 警告表示時間
    }

    // PublisherにURLを渡す (Storageから取得)
    String endpointUrl = storage.getEndpointUrl();
    publisher.begin(endpointUrl); // URLが空でもエラーにはならない

    if (!pulseCounter.begin()) { display.showMessage("PCNT Init FAIL!", 2); delay(3000); /* 必要なら停止 */ }

    metrics.begin(); // 累積データロード (SDから)

    wifi.begin();    // WiFi初期化 (自動接続試行 NVS->JSON[0])

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("Wake up from Deep Sleep (EXT0 - Pulse)");
        currentState = AppState::IDLE_DISPLAY; sessionActive = false;
        M5.Lcd.wakeup(); M5.Lcd.setBrightness(100); M5.Speaker.tone(500, 100);
    } else {
        Serial.printf("Normal boot (Wakeup Cause: %d)\n", wakeup_reason);
        currentState = AppState::IDLE_DISPLAY; sessionActive = false;
        M5.Lcd.setBrightness(100);
    }

    display.showMessage("Setup Complete", 2, true); delay(1000);
    Serial.println("Setup Complete. Entering main loop...");
}


// --- 状態別ハンドラ関数プロトタイプ ---
void handleIdleState(unsigned long currentMillis);
void handleTrackingState(unsigned long currentMillis);
void handleWifiSetupState(unsigned long currentMillis);
void handleWifiConnectingState(unsigned long currentMillis);
void handleWifiScanningState(unsigned long currentMillis);
void handleAPConfigState(unsigned long currentMillis);


// --- Arduino Loop ---
void loop() {
    unsigned long currentMillis = millis();
    M5.update(); // ボタン状態更新は最初に

    // --- APモードがアクティブなら専用処理 ---
    if (apPortal.isActive()) {
        handleAPConfigState(currentMillis); // APモードハンドラ呼び出し
    } else {
        // --- APモードでない場合の通常処理 ---
        wifi.updateStatus(); // WiFi接続状態更新 (STAモード時)
        metrics.update(currentMillis); // 計測データ更新

        // ★★★ Wi-Fi接続時にNTP同期を試みる ★★★
        if (wifi.isConnected()) {
             initNtp(); // 同期済み or 一定時間経過していたら再同期を試みる
        } else {
             // Wi-Fiが切断されたら、同期フラグをリセット
             if (timeSynchronized) {
                 Serial.println("WiFi disconnected, NTP sync status reset.");
                 timeSynchronized = false;
             }
        }

        // UI状態更新 (Movement detection)
        // WiFi関連画面表示中は自動でTracking/Idle画面に戻らないようにする
        if (currentState != AppState::WIFI_SETUP && currentState != AppState::WIFI_CONNECTING &&
            currentState != AppState::WIFI_SCANNING && currentState != AppState::WIFI_AP_CONFIG)
        {
            bool isCurrentlyMoving = metrics.isMoving();
            if (!sessionActive && isCurrentlyMoving) {
                Serial.println("Main: Session activated by movement.");
                sessionActive = true;
                currentState = AppState::TRACKING_DISPLAY;
                M5.Lcd.wakeup(); M5.Lcd.setBrightness(100);
            } else if (sessionActive && !isCurrentlyMoving) {
                Serial.println("Main: Session deactivated by timeout.");
                sessionActive = false;
                currentState = AppState::IDLE_DISPLAY;
            }
        }


        // 状態別ハンドラ呼び出し
        switch (currentState) {
            case AppState::IDLE_DISPLAY:      handleIdleState(currentMillis);       break;
            case AppState::TRACKING_DISPLAY:  handleTrackingState(currentMillis);   break;
            case AppState::WIFI_SETUP:        handleWifiSetupState(currentMillis);  break;
            case AppState::WIFI_CONNECTING:   handleWifiConnectingState(currentMillis); break;
            case AppState::WIFI_SCANNING:     handleWifiScanningState(currentMillis); break;
            // WIFI_AP_CONFIG は isActive() で処理される
            case AppState::SLEEPING:          /* スリープ移行処理 */                 break;
            case AppState::INITIALIZING:      /* 通常ここには来ない */               break;
            default: break;
        }

        // データ送信 (TRACKING中のみ)
        if (sessionActive && currentState == AppState::TRACKING_DISPLAY && wifi.isConnected()) {
            publisher.publishIfNeeded(metrics.getData());
        }

        // スリープ移行判定 (Idle状態でのみ)
        if (currentState == AppState::IDLE_DISPLAY) {
             unsigned long lastPulseTime = metrics.getLastPulseObservedMs();
             bool shouldSleep = false;
             if (lastPulseTime > 0) { // 過去にパルスあり
                 if (currentMillis - lastPulseTime > SLEEP_TIMEOUT_MS) {
                     shouldSleep = true;
                     Serial.println("Main: Idle & Sleep Timeout after last pulse.");
                 }
             } else { // まだ一度もパルスがない
                 if (currentMillis > SLEEP_TIMEOUT_MS) { // 起動からの経過時間で判断
                      shouldSleep = true;
                      Serial.println("Main: Idle & Sleep Timeout since boot.");
                 }
             }
             if (shouldSleep) {
                 Serial.println("Main: Preparing deep sleep.");
                 currentState = AppState::SLEEPING;
             }
        }


         // --- デバッグ用シリアル出力 (★NTP同期状態追加★) ---
         if (currentMillis - lastDebugPrintTime > 2000) {
             unsigned long currentSwCount = pulseCounter.getPulseCount();
             unsigned long lastPulseTimestampFromCounter = pulseCounter.getLastPulseTime();
             unsigned long lastPulseTimestampFromMetrics = metrics.getLastPulseObservedMs();
             int16_t hardware_count = 0;
             esp_err_t err = pcnt_get_counter_value(PCNT_UNIT_0, &hardware_count);
             uint64_t currentTs = getCurrentTimestampMs(); // 現在時刻取得テスト

             if (err == ESP_OK) {
                 Serial.printf("[%llu] HW:%d SW:%lu LastPulse(PC):%lu LastPulse(Met):%lu State:%d WiFi:%d NTP:%d\n",
                                 currentTs, hardware_count, currentSwCount,
                                 lastPulseTimestampFromCounter, lastPulseTimestampFromMetrics,
                                 (int)currentState, wifi.isConnected(), timeSynchronized);
             } else {
                     Serial.printf("[%llu] SW:%lu LastPulse(PC):%lu LastPulse(Met):%lu State:%d WiFi:%d NTP:%d\n",
                                 currentTs, currentSwCount,
                                 lastPulseTimestampFromCounter, lastPulseTimestampFromMetrics,
                                 (int)currentState, wifi.isConnected(), timeSynchronized);
             }
             lastDebugPrintTime = currentMillis;
         }

    } // end if (!apPortal.isActive())


    // --- 画面表示更新 (APモード中でも表示更新が必要なため、ifの外に出す) ---
    display.update(metrics.getData(), currentState, wifi, apPortal);


    // --- スリープ実行 ---
    if (currentState == AppState::SLEEPING) {
        goToDeepSleep();
    }

    delay(10); // Main loop delay
}

// --- 状態別ハンドラ関数の実装 ---

void handleIdleState(unsigned long currentMillis) {
    if (M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) { // B or C でWiFi設定へ
        currentState = AppState::WIFI_SETUP;
        Serial.println("Main: Entering WiFi Setup Mode from Idle.");
        // WiFi設定画面に入ったときのAP起動チェックは handleWifiSetupState で行う
    }
}

void handleTrackingState(unsigned long currentMillis) {
    if (M5.BtnB.pressedFor(1000)) { // B長押しでセッションリセット
        Serial.println("Main: Manual Session Reset requested.");
        metrics.resetSession();
        sessionActive = false; // セッション停止
        currentState = AppState::IDLE_DISPLAY; // Idle画面へ
    } else if (M5.BtnC.wasPressed()) { // CでWiFi設定へ
        currentState = AppState::WIFI_SETUP;
        Serial.println("Main: Entering WiFi Setup Mode from Tracking.");
        // WiFi設定画面に入ったときのAP起動チェックは handleWifiSetupState で行う
    }
}

void handleWifiSetupState(unsigned long currentMillis) {
     // トリガー1: NVSに設定がなければAPモード起動を試みる
     // (ただし、すでにAPモード起動中は何もしない)
     if (!apPortal.isActive()) {
          String temp_ssid, temp_pass;
          if (!storage.loadCredentialsFromNVS(temp_ssid, temp_pass)) {
               Serial.println("Main: No WiFi creds in NVS. Starting AP Portal automatically.");
               if (apPortal.start()) {
                    currentState = AppState::WIFI_AP_CONFIG;
                    return; // APモードに遷移したのでこのハンドラを抜ける
               } else {
                    display.showMessage("AP Start FAIL!", 2, true); delay(1500);
                    // 失敗した場合は WIFI_SETUP に留まる
               }
          }
     }

    // ボタン操作
    if (M5.BtnA.wasPressed()) { // スキャン開始
        Serial.println("Main: Scan requested...");
        display.showMessage("Scanning WiFi...", 1, false);
        int networksFound = wifi.scanNetworks(); // 同期スキャン実行
        currentState = AppState::WIFI_SCANNING; // ★ スキャン結果表示状態へ遷移 ★
        Serial.printf("Main: Scan complete, %d networks found.\n", networksFound);
    }
    // BtnB はここでは何もしない (APモード起動はスキャン後)
    else if (M5.BtnC.wasPressed()) { // 戻る
         currentState = sessionActive ? AppState::TRACKING_DISPLAY : AppState::IDLE_DISPLAY;
         Serial.println("Main: Exiting WiFi Setup via BtnC.");
    }
    // ここで YAMLからの接続試行ボタンを追加することも可能
    // else if (M5.BtnB.pressedFor(1000)) { // 例: B長押し
    //     if (storage.getWifiCredentialCount() > 0) {
    //         Serial.println("Main: Attempting WiFi connection from JSON[0]...");
    //         display.showMessage("Connecting(JSON)...", 1, false);
    //         currentState = AppState::WIFI_CONNECTING;
    //         wifi.connectFromYaml(0); // ★ connectFromJson(0) の方が適切かも ★
    //     } else {
    //         display.showMessage("No WiFi in JSON", 1, true); delay(1000);
    //     }
    // }
}

void handleWifiConnectingState(unsigned long currentMillis) {
    // 接続試行中の処理 (WifiManagerがisConnectingフラグを管理)
    if (!wifi.isAttemptingConnection()) { // 接続試行が終わったら
        if (wifi.isConnected()) { // 接続成功
            currentState = sessionActive ? AppState::TRACKING_DISPLAY : AppState::IDLE_DISPLAY;
            Serial.println("Main: WiFi Connected.");
            display.showMessage("Connected!", 2, true); delay(1000);
        } else { // 接続失敗
            currentState = AppState::WIFI_SETUP;
            Serial.println("Main: WiFi Connection Failed/Timeout.");
            display.showMessage("Connect FAIL!", 2, true); delay(1500);
        }
    }
     // BtnCでキャンセル可能にする
     else if (M5.BtnC.wasPressed()) {
          Serial.println("Main: Cancelling WiFi connection attempt.");
          wifi.disconnect(); // 接続試行を中断
          currentState = AppState::WIFI_SETUP;
     }
}

void handleWifiScanningState(unsigned long currentMillis) {
    // スキャン結果表示中の処理
    if (M5.BtnA.wasPressed()) {
        // TODO: ネットワーク選択 (UI実装が必要)
        Serial.println("Network selection (TODO)");
        display.showMessage("Select TODO", 1, false); delay(1000);
    }
    // ★ トリガー2: スキャン後にBtnBでAPモード開始 ★
    else if (M5.BtnB.wasPressed()) {
        Serial.println("Main: Starting AP Config Portal after scan via BtnB...");
        if (apPortal.start()) {
            currentState = AppState::WIFI_AP_CONFIG; // APモード状態へ
        } else {
            display.showMessage("AP Start FAIL!", 2, true); delay(1500);
            // 失敗したらスキャン結果表示画面に留まる
        }
    } else if (M5.BtnC.wasPressed()) { // 戻る (WiFi設定メニューへ)
        currentState = AppState::WIFI_SETUP;
        Serial.println("Main: Exiting WiFi Scan results via BtnC.");
    }
}

void handleAPConfigState(unsigned long currentMillis) {
    // APモード中の処理
    apPortal.handleClient(); // Webサーバー/DNS処理

    // 設定が保存されたかチェック (APConfigPortal内でフラグが立つ)
    if (apPortal.wereCredentialsSaved()) {
        Serial.println("Main: Credentials saved via AP detected. Restarting...");
        display.showMessage("Saved! Restarting...", 2, true);
        apPortal.resetCredentialsSavedFlag(); // フラグリセット
        delay(1500); // メッセージ表示時間
        apPortal.stop(); // APモード停止
        delay(100);
        ESP.restart(); // 再起動
    }

    // BtnCでのキャンセル
     if (M5.BtnC.wasPressed()) {
          Serial.println("Main: Canceling AP Config Portal via BtnC.");
          apPortal.stop();
          currentState = AppState::WIFI_SETUP; // WiFi設定画面に戻る
     }
}