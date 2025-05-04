#include <M5Stack.h>
#include "config.hpp"
#include "TrackerData.hpp"
#include "Storage.hpp"
#include "PulseCounter.hpp"
#include "MetricsCalculator.hpp"
#include "Display.hpp"
#include "WifiManager.hpp"
#include "DataPublisher.hpp"
#include "APConfigPortal.hpp" // ★ APConfigPortal ヘッダー ★
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
APConfigPortal apPortal(storage, wifi); // ★ APConfigPortal オブジェクト生成 ★

// --- Global State ---
AppState currentState = AppState::INITIALIZING;
// bool sessionActive = false; // ★ 削除: currentState で管理 ★
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
    // スリープ前に最新の累積データをSDに追記（JSON Lines形式）
    Serial.println("Appending history data before sleep...");
    if (!storage.appendHistoryDataToSD(metrics.getData())) {
        Serial.println("Failed to append history data!");
    }
    delay(100); // 書き込み待機
    M5.Lcd.sleep(); // LCDをスリープ

    // --- Wakeup Source Configuration ---
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL); // 全てのソースを無効化
    Serial.printf("Configuring GPIO %d for LOW level wakeup.\n", PULSE_INPUT_PIN);
    // EXT0: 指定したピンのレベル(0=Low, 1=High)で起動
    esp_err_t err = esp_sleep_enable_ext0_wakeup((gpio_num_t)PULSE_INPUT_PIN, 0); // Lowレベルで起動
    if(err != ESP_OK){ Serial.printf("Failed to enable EXT0 wakeup: %s\n", esp_err_to_name(err)); }
    else { Serial.println("EXT0 Wakeup Enabled on LOW level."); }

    // 他の起動要因も必要なら設定 (例: タイマー)
    // esp_sleep_enable_timer_wakeup(10 * 1000000); // 10秒後

    Serial.flush(); // シリアル出力完了待ち
    esp_deep_sleep_start(); // ディープスリープ開始
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

    metrics.begin(); // 累積データロード (SDから) & セッションリセット

    wifi.begin();    // WiFi初期化 (自動接続試行 NVS->JSON[0])

    // 起動要因を確認
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("Wake up from Deep Sleep (EXT0 - Pulse)");
        currentState = AppState::IDLE_DISPLAY; // ディープスリープ復帰時はアイドルから
        M5.Lcd.wakeup(); M5.Lcd.setBrightness(100); // LCD復帰
        M5.Speaker.tone(500, 100); // 復帰音
    } else {
        Serial.printf("Normal boot (Wakeup Cause: %d)\n", wakeup_reason);
        currentState = AppState::IDLE_DISPLAY; // 通常起動時もアイドルから
        M5.Lcd.setBrightness(100); // 輝度設定
    }

    display.showMessage("Setup Complete", 2, true); delay(1000);
    Serial.println("Setup Complete. Entering main loop...");
}


// --- 状態別ハンドラ関数プロトタイプ ---
void handleIdleState(unsigned long currentMillis);
void handleTrackingState(unsigned long currentMillis);
void handleStoppingState(unsigned long currentMillis); // ★ 追加 ★
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
        metrics.update(currentMillis); // 計測データ更新 (isMoving, isTimerRunning がここで更新される)

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

        // ★ 状態遷移ロジックを各ハンドラに移動 ★
        // 状態別ハンドラ呼び出し
        switch (currentState) {
            case AppState::IDLE_DISPLAY:      handleIdleState(currentMillis);       break;
            case AppState::TRACKING_DISPLAY:  handleTrackingState(currentMillis);   break;
            case AppState::STOPPING:          handleStoppingState(currentMillis);   break; // ★ STOPPINGハンドラ呼び出し ★
            case AppState::WIFI_SETUP:        handleWifiSetupState(currentMillis);  break;
            case AppState::WIFI_CONNECTING:   handleWifiConnectingState(currentMillis); break;
            case AppState::WIFI_SCANNING:     handleWifiScanningState(currentMillis); break;
            // WIFI_AP_CONFIG は isActive() で処理される
            case AppState::SLEEPING:          /* スリープ移行処理は loop の最後で */ break;
            case AppState::INITIALIZING:      /* 通常ここには来ない */               break;
            default:
                 // 不明な状態になったらアイドルに戻すなど
                 Serial.printf("Warning: Unknown AppState %d. Resetting to IDLE.\n", (int)currentState);
                 currentState = AppState::IDLE_DISPLAY;
                 break;
        }

        // ★ データ送信条件を TRACKING_DISPLAY のみに変更 ★
        if (currentState == AppState::TRACKING_DISPLAY && wifi.isConnected()) {
            publisher.publishIfNeeded(metrics.getData());
        }

        // スリープ移行判定 (Idle状態でのみ)
        if (currentState == AppState::IDLE_DISPLAY) {
             unsigned long lastPulseTime = metrics.getLastPulseObservedMs();
             bool shouldSleep = false;
             // 起動直後などで lastPulseTime が 0 の場合も考慮
             if (lastPulseTime > 0) { // 過去にペダルを漕いだことがある場合
                 if (currentMillis - lastPulseTime > SLEEP_TIMEOUT_MS) {
                     shouldSleep = true;
                     // Serial.println("Main: Idle & Sleep Timeout after last pulse.");
                 }
             } else { // まだ一度も漕いでいない場合
                 if (currentMillis > SLEEP_TIMEOUT_MS) { // 起動後、一定時間操作がなければスリープ
                      shouldSleep = true;
                      // Serial.println("Main: Idle & Sleep Timeout since boot.");
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
             // pcnt_get_counter_value はユニットを指定する必要がある
             esp_err_t err = pcnt_get_counter_value(PCNT_UNIT, &hardware_count); // PCNT_UNIT_0 を使う
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
             Serial.printf("Current State: %d\n", currentState);
             lastDebugPrintTime = currentMillis;
         }

    } // end if (!apPortal.isActive())


    // --- 画面表示更新 ---
    display.update(metrics.getData(), currentState, wifi, apPortal);


    // --- スリープ実行 ---
    if (currentState == AppState::SLEEPING) {
        goToDeepSleep();
    }

    delay(10); // Main loop delay
}

// --- 状態別ハンドラ関数の実装 ---

void handleIdleState(unsigned long currentMillis) {
    // ★ 動き出したら TRACKING に遷移 ★
    if (metrics.isMoving()) { // isMoving()は SLEEP_TIMEOUT 以内かを見る
        Serial.println("Main: Movement detected from IDLE. Entering TRACKING.");
        currentState = AppState::TRACKING_DISPLAY;
        M5.Lcd.wakeup(); M5.Lcd.setBrightness(100);
        // セッションリセットはしない（MetricsCalculator::update内で新規開始時に処理）
        return; // 状態遷移したので以降の処理はしない
    }

    // ボタン処理
    if (M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) { // B or C でWiFi設定へ
        currentState = AppState::WIFI_SETUP;
        Serial.println("Main: Entering WiFi Setup Mode from Idle.");
    }
}

void handleTrackingState(unsigned long currentMillis) {
    // ★ タイマーが停止したら STOPPING に遷移 ★
    if (!metrics.isTimerRunning()) { // isTimerRunning()は TIMER_STOP_DELAY 以内かを見る
        Serial.println("Main: Timer stopped in TRACKING. Entering STOPPING.");
        currentState = AppState::STOPPING;
        return; // 状態遷移
    }

    // ボタン処理
    if (M5.BtnB.pressedFor(1000)) { // B長押しでセッションリセット -> IDLE へ
        Serial.println("Main: Manual Session Reset requested during TRACKING.");
        metrics.resetSession(); // セッションデータとパルスカウンタをリセット
        currentState = AppState::IDLE_DISPLAY;
    } else if (M5.BtnC.wasPressed()) { // CでWiFi設定へ
        currentState = AppState::WIFI_SETUP;
        Serial.println("Main: Entering WiFi Setup Mode from Tracking.");
    }
}

// ★★★ STOPPING 状態のハンドラを追加 ★★★
void handleStoppingState(unsigned long currentMillis) {
    // ★ 動きが完全に止まったら(SLEEP_TIMEOUT経過) IDLE に遷移 ★
    if (!metrics.isMoving()) {
        Serial.println("Main: Movement stopped in STOPPING. Entering IDLE.");
        // セッション終了処理（MetricsCalculator内で実施済みのはず）
        currentState = AppState::IDLE_DISPLAY;
        return; // 状態遷移
    }

    // ★ 停止中に再度動き出したら TRACKING に戻る ★
    // metrics.update() 内で isMoving=true の時に hadRecentPulse があれば
    // isTimerRunning() も true に戻るはず。それをここで検知する。
    if (metrics.isTimerRunning()){ // isMoving は true のはず
        Serial.println("Main: Movement resumed from STOPPING. Entering TRACKING.");
        currentState = AppState::TRACKING_DISPLAY;
        return; // 状態遷移
    }


    // ボタン処理 (TRACKING と同様)
    if (M5.BtnB.pressedFor(1000)) { // B長押しでセッションリセット -> IDLE へ
        Serial.println("Main: Manual Session Reset requested during STOPPING.");
        metrics.resetSession();
        currentState = AppState::IDLE_DISPLAY;
    } else if (M5.BtnC.wasPressed()) { // CでWiFi設定へ
        currentState = AppState::WIFI_SETUP;
        Serial.println("Main: Entering WiFi Setup Mode from Stopping.");
    }
}

void handleWifiSetupState(unsigned long currentMillis) {
     // トリガー1: NVSに設定がなければAPモード起動を試みる
     if (!apPortal.isActive()) {
          String temp_ssid, temp_pass;
          if (!storage.loadCredentialsFromNVS(temp_ssid, temp_pass)) {
               Serial.println("Main: No WiFi creds in NVS. Starting AP Portal automatically.");
               if (apPortal.start()) {
                    currentState = AppState::WIFI_AP_CONFIG;
                    return;
               } else {
                    display.showMessage("AP Start FAIL!", 2, true); delay(1500);
               }
          }
     }
    // ボタン操作
    if (M5.BtnA.wasPressed()) { // スキャン開始
        Serial.println("Main: Scan requested...");
        display.showMessage("Scanning WiFi...", 1, false);
        int networksFound = wifi.scanNetworks();
        currentState = AppState::WIFI_SCANNING;
        Serial.printf("Main: Scan complete, %d networks found.\n", networksFound);
    } else if (M5.BtnC.wasPressed()) { // 戻る
         // 遷移元が TRACKING or STOPPING だった可能性も考慮
         if (metrics.isMoving()){ //まだスリープタイムアウト前なら
              currentState = metrics.isTimerRunning() ? AppState::TRACKING_DISPLAY : AppState::STOPPING;
         } else { // 完全に停止していたら
              currentState = AppState::IDLE_DISPLAY;
         }
         Serial.println("Main: Exiting WiFi Setup via BtnC.");
    }
     // YAMLからの接続試行ボタンなどをBtnB長押しなどに割り当てることも可能
     else if (M5.BtnB.pressedFor(1000)) { // 例: B長押しでJSON[0]に接続試行
         if (storage.getWifiCredentialCount() > 0) {
             Serial.println("Main: Attempting WiFi connection from JSON[0]...");
             display.showMessage("Connecting(JSON)...", 1, false);
             currentState = AppState::WIFI_CONNECTING;
             wifi.connectFromYaml(0); // JSONの最初の設定で接続
         } else {
             display.showMessage("No WiFi in JSON", 1, true); delay(1000);
         }
     }
}

void handleWifiConnectingState(unsigned long currentMillis) {
    // 接続試行中の処理
    if (!wifi.isAttemptingConnection()) { // 試行完了
        if (wifi.isConnected()) {
            // 接続成功したら元の状態（TRACKING/STOPPING/IDLE）に戻る
            if (metrics.isMoving()){
                 currentState = metrics.isTimerRunning() ? AppState::TRACKING_DISPLAY : AppState::STOPPING;
            } else {
                 currentState = AppState::IDLE_DISPLAY;
            }
            Serial.println("Main: WiFi Connected.");
            display.showMessage("Connected!", 2, true); delay(1000);
        } else { // 接続失敗
            currentState = AppState::WIFI_SETUP; // 設定画面に戻る
            Serial.println("Main: WiFi Connection Failed/Timeout.");
            display.showMessage("Connect FAIL!", 2, true); delay(1500);
        }
    } else if (M5.BtnC.wasPressed()) { // 接続試行中にキャンセル
          Serial.println("Main: Cancelling WiFi connection attempt.");
          wifi.disconnect(); // 接続試行を中断
          currentState = AppState::WIFI_SETUP; // 設定画面に戻る
     }
}

void handleWifiScanningState(unsigned long currentMillis) {
    // スキャン結果表示中の処理
    if (M5.BtnA.wasPressed()) {
        // TODO: ネットワーク選択UI
        Serial.println("Network selection (TODO)");
        display.showMessage("Select TODO", 1, false); delay(1000);
    } else if (M5.BtnB.wasPressed()) { // トリガー2: APモード開始
        Serial.println("Main: Starting AP Config Portal after scan via BtnB...");
        if (apPortal.start()) {
            currentState = AppState::WIFI_AP_CONFIG;
        } else {
            display.showMessage("AP Start FAIL!", 2, true); delay(1500);
        }
    } else if (M5.BtnC.wasPressed()) { // 戻る (WiFi設定メニューへ)
        currentState = AppState::WIFI_SETUP;
        Serial.println("Main: Exiting WiFi Scan results via BtnC.");
    }
}

void handleAPConfigState(unsigned long currentMillis) {
    // APモード中の処理
    apPortal.handleClient(); // Webサーバー/DNS処理

    // 設定が保存されたかチェック
    if (apPortal.wereCredentialsSaved()) {
        Serial.println("Main: Credentials saved via AP detected. Restarting...");
        display.showMessage("Saved! Restarting...", 2, true);
        apPortal.resetCredentialsSavedFlag(); // フラグリセット
        delay(1500);
        apPortal.stop();
        delay(100);
        ESP.restart(); // ★ 再起動して新しい設定で接続 ★
    }

    // BtnCでのキャンセル
     if (M5.BtnC.wasPressed()) {
          Serial.println("Main: Canceling AP Config Portal via BtnC.");
          apPortal.stop();
          currentState = AppState::WIFI_SETUP; // WiFi設定画面に戻る
     }
}