#include "Display.hpp"
#include "WifiManager.hpp" // WifiManager ヘッダをインクルード
#include "APConfigPortal.hpp" // APConfigPortal ヘッダー
#include <stdio.h>
#include <WiFi.h>

// コンストラクタ
Display::Display() : sprite(&M5.Lcd) {} // M5.LcdをSpriteの描画先に指定

// 初期化
void Display::begin() {
    sprite.setColorDepth(8); // 色深度(8bit=256色)
    // Sprite作成 (画面サイズいっぱい)
    if (sprite.createSprite(M5.Lcd.width(), M5.Lcd.height()) == nullptr) {
        Serial.println("Sprite creation failed! Check memory?");
    } else {
        Serial.println("Sprite created successfully.");
    }
    sprite.setTextFont(2); // デフォルトフォント設定 (Free Sans 9pt)
    sprite.setTextColor(TFT_WHITE, TFT_BLACK); // デフォルト色設定 (白文字、黒背景)
    sprite.setTextSize(1); // デフォルト文字サイズ設定
    sprite.setTextDatum(TL_DATUM); // デフォルトの文字基準位置を左上(Top Left)に
}

// Spriteクリア (画面にはまだ反映されない)
void Display::clear() {
    sprite.fillSprite(BLACK);
}

// メッセージを画面中央に表示
void Display::showMessage(const String& msg, int size, bool clearScreen) {
    if (clearScreen) sprite.fillSprite(BLACK); // 必要なら画面クリア
    sprite.setTextSize(size);         // 指定された文字サイズ
    sprite.setTextDatum(MC_DATUM);    // 文字基準位置を中央(Middle Center)に
    sprite.drawString(msg, sprite.width() / 2, sprite.height() / 2); // 画面中央に描画
    sprite.pushSprite(0, 0);          // Spriteの内容をLCDに即時反映
    sprite.setTextSize(1);            // デフォルト文字サイズに戻す
    sprite.setTextDatum(TL_DATUM);    // デフォルトの文字基準位置に戻す
}

// ミリ秒を HH:MM:SS 形式の文字列に変換
String Display::formatTime(unsigned long ms) {
    unsigned long seconds = ms / 1000;
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    char buf[10];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s); // ゼロ埋め2桁
    return String(buf);
}

// ミリ秒を累積時間表示 (Xd Yh Zm または Yh Zm) 形式の文字列に変換
String Display::formatCumulativeTime(uint64_t ms) {
    unsigned long seconds = ms / 1000;
    int d = seconds / 86400;           // 日数
    int h = (seconds % 86400) / 3600;  // 時間
    int m = (seconds % 3600) / 60;     // 分
    char buf[20];
    if (d > 0) { // 1日以上の場合
        snprintf(buf, sizeof(buf), "%dd %dh %dm", d, h, m);
    } else {     // 1日未満の場合
        snprintf(buf, sizeof(buf), "%dh %dm", h, m);
    }
    return String(buf);
}


// ★★★ updateメソッドに STOPPING ケースを追加 ★★★
void Display::update(const TrackerData& data, AppState state, WifiManager& wifiManager, APConfigPortal& apPortal) {
    // if (sprite.width() == 0) return; // beginで確保済み想定

    switch(state) {
        case AppState::TRACKING_DISPLAY: displayTrackingScreen(data, wifiManager); break;
        case AppState::IDLE_DISPLAY:     displayIdleScreen(data, wifiManager); break;
        case AppState::STOPPING:         displayStoppingScreen(data, wifiManager); break; // ★ STOPPING画面表示呼び出し ★
        case AppState::WIFI_CONNECTING:  displayWifiScreen(wifiManager); break;
        case AppState::WIFI_SETUP:       displayWifiScreen(wifiManager); break;
        case AppState::WIFI_SCANNING:    displayScanResultsScreen(wifiManager); break;
        case AppState::WIFI_AP_CONFIG:   displayAPConfigScreen(apPortal); break;
        case AppState::INITIALIZING:     break; // setupでshowMessageされるので何もしない
        case AppState::SLEEPING:         M5.Lcd.sleep(); return; // スリープならLCDをOFFにして終了
        default:                         sprite.fillSprite(BLACK); break; // 不明な状態なら画面クリア
    }

    // スリープと初期化以外ならSpriteの内容をLCDに転送
    if (state != AppState::INITIALIZING && state != AppState::SLEEPING) {
        sprite.pushSprite(0, 0);
    }
}

// トラッキング中の画面描画
void Display::displayTrackingScreen(const TrackerData& data, WifiManager& wifiManager) {
    sprite.fillSprite(BLACK);
    sprite.setTextDatum(TL_DATUM); // 左上基準に戻す
    sprite.setTextSize(1); sprite.setTextColor(TFT_WHITE, TFT_BLACK); sprite.setTextFont(2);
    sprite.setCursor(5, 5); sprite.setTextColor(TFT_GREEN, TFT_BLACK); sprite.print("TRACKING");

    // WiFi Status (右上に表示)
    sprite.setTextDatum(TR_DATUM); // 右上基準
    sprite.setCursor(sprite.width() - 5, 5);
    if (wifiManager.isConnected()) { sprite.setTextColor(TFT_GREEN, TFT_BLACK); sprite.print("WiFi OK"); }
    else { sprite.setTextColor(TFT_RED, TFT_BLACK); sprite.print("WiFi NO"); }
    sprite.setTextDatum(TL_DATUM); // 左上基準に戻す
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);

    // --- メトリクス表示 ---
    int row1_y = 30; int row2_y = 80; int row3_y = 130; int col1_x = 10; int col2_x = 170; int label_y_offset = 28;
    // Time
    sprite.setCursor(col1_x, row1_y); sprite.setTextSize(2); sprite.print(formatTime(data.sessionElapsedTimeMs));
    sprite.setTextSize(1); sprite.setCursor(col1_x, row1_y + label_y_offset); sprite.print("Time");
    // RPM
    sprite.setCursor(col2_x, row1_y); sprite.setTextSize(2); sprite.printf("%.0f", data.currentRpm);
    sprite.setTextSize(1); sprite.setCursor(col2_x, row1_y + label_y_offset); sprite.print("RPM");
    // Speed
    sprite.setCursor(col1_x, row2_y); sprite.setTextSize(2); sprite.printf("%.1f", data.currentSpeedKmh);
    sprite.setTextSize(1); sprite.setCursor(col1_x, row2_y + label_y_offset); sprite.print("Speed km/h");
    // Distance
    sprite.setCursor(col2_x, row2_y); sprite.setTextSize(2); sprite.printf("%.2f", data.sessionDistanceKm);
    sprite.setTextSize(1); sprite.setCursor(col2_x, row2_y + label_y_offset); sprite.print("Dist km");
    // Calories
    sprite.setCursor(col1_x, row3_y); sprite.setTextSize(2); sprite.printf("%.1f", data.sessionCaloriesKcal);
    sprite.setTextSize(1); sprite.setCursor(col1_x, row3_y + label_y_offset); sprite.print("Cal kcal");
    // METs (Optional)
    // sprite.setCursor(col2_x, row3_y); sprite.setTextSize(2); sprite.printf("%.1f", data.currentMets);
    // sprite.setTextSize(1); sprite.setCursor(col2_x, row3_y + label_y_offset); sprite.print("METs");

    // --- フッター (累積データ) ---
    sprite.drawFastHLine(0, sprite.height() - 30, sprite.width(), TFT_DARKGREY); // 区切り線
    sprite.setCursor(5, sprite.height() - 20); sprite.setTextSize(1);
    uint64_t displayTotalTimeMs = data.cumulativeTimeMs;
    float displayTotalDistKm = data.cumulativeDistanceKm;
    float displayTotalCalKcal = data.cumulativeCaloriesKcal;
    // 累積時間を加算表示する場合 (オプション)
    // displayTotalTimeMs += data.sessionElapsedTimeMs;
    sprite.printf("Total: %s | %.1fkm | %.0fkcal", formatCumulativeTime(displayTotalTimeMs).c_str(), displayTotalDistKm, displayTotalCalKcal);
}

// アイドル中の画面描画
void Display::displayIdleScreen(const TrackerData& data, WifiManager& wifiManager) {
    sprite.fillSprite(BLACK);
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.setTextFont(4); sprite.setTextSize(2); // 大きめのフォント
    sprite.setTextDatum(MC_DATUM); // 中央揃え
    sprite.drawString("IDLE", sprite.width()/2, 30); // 画面上部中央に "IDLE"

    // WiFi Status (右上に表示)
    sprite.setTextFont(2); sprite.setTextSize(1); // 通常フォントに戻す
    sprite.setTextDatum(TR_DATUM); // 右上基準
    sprite.setCursor(sprite.width() - 5, 5);
    if (wifiManager.isConnected()) { sprite.setTextColor(TFT_GREEN, TFT_BLACK); sprite.print("WiFi OK"); }
    else { sprite.setTextColor(TFT_RED, TFT_BLACK); sprite.print("WiFi NO"); }
    sprite.setTextDatum(TL_DATUM); // 左上基準に戻す
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);

    // --- 累積データ表示 ---
    sprite.setCursor(10, 60); sprite.println("Cumulative Stats:");
    sprite.setCursor(10, 90); sprite.print("Total Time: " + formatCumulativeTime(data.cumulativeTimeMs));
    sprite.setCursor(10, 120); sprite.printf("Total Dist: %.1f Km", data.cumulativeDistanceKm);
    sprite.setCursor(10, 150); sprite.printf("Total Cal: %.0f Kcal", data.cumulativeCaloriesKcal);

    // --- フッター ---
    sprite.setTextDatum(MC_DATUM); // 中央揃え
    sprite.drawString("Pedal to start", sprite.width() / 2, sprite.height() - 45);
    sprite.setTextSize(1);
    sprite.drawString("BtnB/C: WiFi Setup", sprite.width() / 2, sprite.height() - 20); // ボタン説明
    sprite.setTextDatum(TL_DATUM); // 左上基準に戻す
}

// ★★★ STOPPING 状態の画面表示関数 ★★★
void Display::displayStoppingScreen(const TrackerData& data, WifiManager& wifiManager) {
    sprite.fillSprite(TFT_DARKGREY); // 背景色を少し変える (例: ダークグレー)
    sprite.setTextDatum(TL_DATUM); // 左上基準に戻す
    sprite.setTextSize(1); sprite.setTextColor(TFT_YELLOW, TFT_DARKGREY); sprite.setTextFont(2);
    sprite.setCursor(5, 5); sprite.print("PAUSED"); // 状態表示

    // WiFi Status (右上に表示)
    sprite.setTextDatum(TR_DATUM); // 右上基準
    sprite.setCursor(sprite.width() - 5, 5);
    if (wifiManager.isConnected()) { sprite.setTextColor(TFT_GREEN, TFT_DARKGREY); sprite.print("WiFi OK"); }
    else { sprite.setTextColor(TFT_RED, TFT_DARKGREY); sprite.print("WiFi NO"); }
    sprite.setTextDatum(TL_DATUM); // 左上基準に戻す
    sprite.setTextColor(TFT_WHITE, TFT_DARKGREY); // 文字色を白に戻す

    // --- 停止時のセッションサマリ表示 ---
    int y_start = 40;
    int line_h = 30;
    sprite.setCursor(10, y_start); sprite.print("Session Summary:");
    sprite.setCursor(10, y_start + line_h * 1); sprite.print(" Time: " + formatTime(data.sessionElapsedTimeMs)); // 停止した時間
    sprite.setCursor(10, y_start + line_h * 2); sprite.printf(" Dist: %.2f Km", data.sessionDistanceKm);
    sprite.setCursor(10, y_start + line_h * 3); sprite.printf(" Cal : %.1f Kcal", data.sessionCaloriesKcal);

    // --- フッター ---
    sprite.setTextDatum(MC_DATUM); // 中央揃え
    sprite.drawString("Waiting for sleep...", sprite.width() / 2, sprite.height() - 45);
    sprite.setTextSize(1);
    sprite.drawString("BtnB(Long):Reset / BtnC:WiFi", sprite.width() / 2, sprite.height() - 20); // ボタン説明
    sprite.setTextDatum(TL_DATUM); // 左上基準に戻す
}


// Wi-Fi設定メニュー画面
void Display::displayWifiScreen(WifiManager& wifiManager) {
    sprite.fillSprite(TFT_NAVY); // 背景色
    sprite.setTextColor(TFT_WHITE, TFT_NAVY); // 文字色、背景色
    sprite.setTextFont(4); sprite.setTextSize(1); // タイトル用フォント
    sprite.setTextDatum(MC_DATUM); // 中央揃え
    sprite.drawString("Wi-Fi Setup", sprite.width()/2, 20);

    sprite.setTextFont(2); sprite.setTextSize(1); // 通常フォント
    sprite.setTextDatum(TL_DATUM); // 左上基準
    sprite.setCursor(10, 50);
    sprite.println(wifiManager.getStatusMessage()); // WifiManagerからステータス取得・表示

    // Button hints (画面下部中央揃え)
    sprite.setTextDatum(BC_DATUM);
    sprite.drawString("A:Scan / B:Use AP / C:Back", sprite.width() / 2, sprite.height() - 10);
    sprite.setTextDatum(TL_DATUM); // 元に戻す
}

// Wi-Fiスキャン結果画面
void Display::displayScanResultsScreen(WifiManager& wifiManager) {
    sprite.fillSprite(TFT_BLACK);
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.setTextFont(2);
    sprite.setTextSize(1);
    sprite.setTextDatum(MC_DATUM); // 中央揃え
    sprite.drawString("Scan Results", sprite.width()/2, 10);
    sprite.setTextDatum(TL_DATUM); // 左上基準に戻す

    int networkCount = wifiManager.getScanResultCount();
    if (networkCount <= 0) {
        sprite.setCursor(10, 40);
        sprite.print(wifiManager.getStatusMessage()); // 例: "Scanning..." or "No networks found"
    } else {
        int yPos = 25;          // リスト開始Y座標
        int lineHeight = 18;    // 各行の高さ
        int maxLines = (sprite.height() - yPos - 30) / lineHeight; // 表示可能な最大行数 (フッター考慮)

        for (int i = 0; i < networkCount && i < maxLines; ++i) {
            WiFiScanInfo info = wifiManager.getScanResult(i);
            sprite.setCursor(5, yPos + i * lineHeight);
            // 暗号化タイプ表示 (* または スペース)
            sprite.print((info.encryptionType == WIFI_AUTH_OPEN) ? "  " : "* ");
            // SSID表示 (長すぎる場合は省略)
            String ssid = info.ssid;
            int maxSsidPixelWidth = sprite.width() - 5 - sprite.textWidth("* ") - sprite.textWidth("-100dBm") - 10; // SSID表示幅計算
            while(sprite.textWidth(ssid) > maxSsidPixelWidth && ssid.length() > 2) {
                 ssid = ssid.substring(0, ssid.length() - 1);
            }
            if (sprite.textWidth(ssid) > maxSsidPixelWidth && ssid.length() > 2) { // さらに短縮が必要なら ".." 付与
                 ssid = ssid.substring(0, ssid.length() - 2) + "..";
            }
            sprite.print(ssid);
            // RSSI表示 (右寄せ)
            String rssiStr = String(info.rssi) + "dBm";
            sprite.setTextDatum(TR_DATUM); // 右上基準 (座標は右上を指定)
            sprite.drawString(rssiStr, sprite.width() - 5, yPos + i * lineHeight);
            sprite.setTextDatum(TL_DATUM); // 左上基準に戻す
        }
        // 全件表示しきれない場合
        if (networkCount > maxLines) {
             sprite.setCursor(5, sprite.height() - 55); // フッターの上あたり
             sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
             sprite.print("More networks... (Scroll N/A)");
             sprite.setTextColor(TFT_WHITE, TFT_BLACK);
        }
    }
    // Button hints (画面下部中央揃え)
    sprite.setTextDatum(BC_DATUM);
    sprite.drawString("A:Select(TODO) / B:Use AP / C:Back", sprite.width() / 2, sprite.height() - 10);
    sprite.setTextDatum(TL_DATUM); // 元に戻す
}

// Wi-Fi AP設定モード中の画面
void Display::displayAPConfigScreen(APConfigPortal& apPortal) {
    sprite.fillSprite(TFT_DARKCYAN); // 背景色
    sprite.setTextColor(TFT_WHITE, TFT_DARKCYAN); // 文字色、背景色
    sprite.setTextFont(4); sprite.setTextSize(1); // タイトル用フォント
    sprite.setTextDatum(MC_DATUM); // 中央揃え
    sprite.drawString("AP Config Mode", sprite.width()/2, 20);

    sprite.setTextFont(2); sprite.setTextSize(1); // 通常フォント
    sprite.setTextDatum(TL_DATUM); // 左上基準
    sprite.setCursor(10, 50);
    sprite.printf("Connect to WiFi:");
    sprite.setCursor(10, 70);
    sprite.setTextSize(2); // SSIDは大きく
    sprite.print(AP_SETUP_SSID); // config.hpp で定義したSSIDを表示
    sprite.setTextSize(1); // サイズ戻す

    sprite.setCursor(10, 110);
    sprite.print("Open browser to:");
    sprite.setCursor(10, 130);
    sprite.setTextSize(2); // IPアドレスも大きく
    sprite.print("http://192.168.4.1"); // M5StackのデフォルトAP IP
    sprite.setTextSize(1); // サイズ戻す

    // Button hints (画面下部中央揃え)
    sprite.setTextDatum(BC_DATUM);
    sprite.drawString("Waiting... / C:Cancel", sprite.width() / 2, sprite.height() - 10);
    sprite.setTextDatum(TL_DATUM); // 元に戻す
}