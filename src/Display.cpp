#include "Display.hpp"
#include "WifiManager.hpp" // ★ WifiManager ヘッダをインクルード ★
#include "APConfigPortal.hpp" // ★ APConfigPortal ヘッダー ★
#include <stdio.h>
#include <WiFi.h>

// コンストラクタ (応答 #84 と同じ)
Display::Display() : sprite(&M5.Lcd) {}

// begin, clear, showMessage, formatTime, formatCumulativeTime (応答 #84 と同じ)
void Display::begin() {
    sprite.setColorDepth(8);
    // sprite.createSprite は M5.begin() 内で処理されることが多いので、
    // ここでのチェックは不要かもしれない (M5GFXの場合)
    // if (sprite.createSprite(M5.Lcd.width(), M5.Lcd.height()) == nullptr) {
    //     Serial.println("Sprite creation failed! Check memory?");
    // } else {
    //     Serial.println("Sprite created successfully.");
    // }
    sprite.setTextFont(2); // デフォルトフォント設定
    sprite.setTextColor(TFT_WHITE, TFT_BLACK); // デフォルト色設定
    sprite.setTextSize(1); // デフォルトサイズ設定
    sprite.createSprite(M5.Lcd.width(), M5.Lcd.height()); // Sprite確保
}
void Display::clear() {
    // if (sprite.width() == 0) return; // createSpriteで確保されるはず
    sprite.fillSprite(BLACK);
    // sprite.pushSprite(0, 0); // clearだけではpushしない方が良いかも
}
void Display::showMessage(const String& msg, int size, bool clearScreen) {
    // if (sprite.width() == 0) return;
    if (clearScreen) sprite.fillSprite(BLACK);
    sprite.setTextSize(size);
    sprite.setTextDatum(MC_DATUM); // 中央揃え
    // int16_t textWidth = sprite.textWidth(msg); // 不要に
    // sprite.setCursor((sprite.width() - textWidth) / 2, sprite.height() / 2 - 10); // 不要に
    sprite.drawString(msg, sprite.width() / 2, sprite.height() / 2); // 中央に描画
    sprite.pushSprite(0, 0); // すぐに表示
    sprite.setTextSize(1); // デフォルトサイズに戻す
    sprite.setTextDatum(TL_DATUM); // デフォルトの左上揃えに戻す
}
String Display::formatTime(unsigned long ms) {
    unsigned long seconds = ms / 1000; int h = seconds / 3600; int m = (seconds % 3600) / 60; int s = seconds % 60;
    char buf[10]; snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s); return String(buf);
}
String Display::formatCumulativeTime(uint64_t ms) {
    unsigned long seconds = ms / 1000; int d = seconds / 86400; int h = (seconds % 86400) / 3600; int m = (seconds % 3600) / 60;
    char buf[20]; if (d > 0) { snprintf(buf, sizeof(buf), "%dd %dh %dm", d, h, m); } else { snprintf(buf, sizeof(buf), "%dh %dm", h, m); } return String(buf);
}

// ★★★ updateメソッドの引数に APConfigPortal& を追加 ★★★
void Display::update(const TrackerData& data, AppState state, WifiManager& wifiManager, APConfigPortal& apPortal) {
    // if (sprite.width() == 0) return; // Spriteが有効かチェック (beginで確保済み想定)

    switch(state) {
        case AppState::TRACKING_DISPLAY:
            displayTrackingScreen(data, wifiManager);
            break;
        case AppState::IDLE_DISPLAY:
            displayIdleScreen(data, wifiManager);
            break;
        case AppState::WIFI_CONNECTING: // 接続中画面
            displayWifiScreen(wifiManager); // WifiManagerのステータスを使う
            break;
        case AppState::WIFI_SETUP:
             // WifiManager から現在のステータスを取得して表示
            displayWifiScreen(wifiManager);
            break;
        // ★★★ スキャン結果表示状態 ★★★
        case AppState::WIFI_SCANNING:
            // WifiManager オブジェクトを渡して結果を描画させる
            displayScanResultsScreen(wifiManager);
            break;
        // ★★★ APモード設定状態 ★★★
        case AppState::WIFI_AP_CONFIG:
            displayAPConfigScreen(apPortal); // APPortalのステータスを使う
            break;
        case AppState::INITIALIZING:
            // 通常 update が呼ばれる前に setup で showMessage される
            break;
        case AppState::SLEEPING:
             M5.Lcd.sleep(); // LCDをスリープ
             return; // pushSprite しないで抜ける
        default:
            sprite.fillSprite(BLACK); // 不明な状態ならクリア
            break;
    }

    // スリープと初期化以外ならSpriteを画面に転送
    if (state != AppState::INITIALIZING && state != AppState::SLEEPING) {
        sprite.pushSprite(0, 0);
    }
}

// displayTrackingScreen, displayIdleScreen の引数とWiFi状態表示を変更
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

    // --- メトリクス表示 (変更なし) ---
    int row1_y = 30; int row2_y = 80; int row3_y = 130; int col1_x = 10; int col2_x = 170; int label_y_offset = 28;
    sprite.setCursor(col1_x, row1_y); sprite.setTextSize(2); sprite.print(formatTime(data.sessionElapsedTimeMs)); sprite.setTextSize(1); sprite.setCursor(col1_x, row1_y + label_y_offset); sprite.print("Time");
    sprite.setCursor(col2_x, row1_y); sprite.setTextSize(2); sprite.printf("%.0f", data.currentRpm); sprite.setTextSize(1); sprite.setCursor(col2_x, row1_y + label_y_offset); sprite.print("RPM");
    sprite.setCursor(col1_x, row2_y); sprite.setTextSize(2); sprite.printf("%.1f", data.currentSpeedKmh); sprite.setTextSize(1); sprite.setCursor(col1_x, row2_y + label_y_offset); sprite.print("Speed km/h");
    sprite.setCursor(col2_x, row2_y); sprite.setTextSize(2); sprite.printf("%.2f", data.sessionDistanceKm); sprite.setTextSize(1); sprite.setCursor(col2_x, row2_y + label_y_offset); sprite.print("Dist km");
    sprite.setCursor(col1_x, row3_y); sprite.setTextSize(2); sprite.printf("%.1f", data.sessionCaloriesKcal); sprite.setTextSize(1); sprite.setCursor(col1_x, row3_y + label_y_offset); sprite.print("Cal kcal");

    // --- フッター (変更なし) ---
    sprite.drawFastHLine(0, sprite.height() - 30, sprite.width(), TFT_DARKGREY); sprite.setCursor(5, sprite.height() - 20); sprite.setTextSize(1);
    uint64_t displayTotalTimeMs = data.cumulativeTimeMs; float displayTotalDistKm = data.cumulativeDistanceKm; float displayTotalCalKcal = data.cumulativeCaloriesKcal;
    sprite.printf("Total: %s | %.1fkm | %.0fkcal", formatCumulativeTime(displayTotalTimeMs).c_str(), displayTotalDistKm, displayTotalCalKcal);
}
void Display::displayIdleScreen(const TrackerData& data, WifiManager& wifiManager) {
    sprite.fillSprite(BLACK);
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.setTextFont(4); sprite.setTextSize(2);
    sprite.setTextDatum(MC_DATUM); // 中央揃え
    sprite.drawString("IDLE", sprite.width()/2, 30);

    // WiFi Status (右上に表示)
    sprite.setTextFont(2); sprite.setTextSize(1);
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
    sprite.drawString("BtnB/C: WiFi Setup", sprite.width() / 2, sprite.height() - 20);
    sprite.setTextDatum(TL_DATUM); // 左上基準に戻す
}

// displayWifiScreen (引数変更、内容をWifiManagerから取得)
void Display::displayWifiScreen(WifiManager& wifiManager) {
    sprite.fillSprite(TFT_NAVY);
    sprite.setTextColor(TFT_WHITE, TFT_NAVY);
    sprite.setTextFont(4); sprite.setTextSize(1);
    sprite.setTextDatum(MC_DATUM); // 中央揃え
    sprite.drawString("Wi-Fi Setup", sprite.width()/2, 20);

    sprite.setTextFont(2); sprite.setTextSize(1);
    sprite.setTextDatum(TL_DATUM); // 左上基準
    sprite.setCursor(10, 50);
    sprite.println(wifiManager.getStatusMessage()); // WifiManagerからステータス取得

    // Button hints
    sprite.setTextDatum(BC_DATUM); // 下中央揃え
    sprite.drawString("A:Scan / B:Use AP / C:Back", sprite.width() / 2, sprite.height() - 10);
    sprite.setTextDatum(TL_DATUM); // 左上基準に戻す
}


// ★★★ スキャン結果表示画面の実装 (★ BtnBの説明をAP起動に変更 ★) ★★★
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
        sprite.print(wifiManager.getStatusMessage()); // "Scanning..." or "No networks found" or "Scan failed"
    } else {
        int yPos = 25;
        int lineHeight = 18;
        int maxLines = (sprite.height() - yPos - 30) / lineHeight; // フッター領域確保

        for (int i = 0; i < networkCount && i < maxLines; ++i) {
            WiFiScanInfo info = wifiManager.getScanResult(i);
            sprite.setCursor(5, yPos + i * lineHeight);
            sprite.print((info.encryptionType == WIFI_AUTH_OPEN) ? "  " : "* ");
            // SSIDが長すぎる場合の省略処理
            String ssid = info.ssid;
            int maxSsidPixelWidth = sprite.width() - 5 - sprite.textWidth("* ") - sprite.textWidth("-100dBm") - 10;
            while(sprite.textWidth(ssid) > maxSsidPixelWidth && ssid.length() > 2) {
                 ssid = ssid.substring(0, ssid.length() - 1);
            }
            // 必要なら末尾に ".." をつける (さらに短縮が必要な場合)
            if (sprite.textWidth(ssid) > maxSsidPixelWidth && ssid.length() > 2) {
                 ssid = ssid.substring(0, ssid.length() - 2) + "..";
            }
            sprite.print(ssid);
            // RSSI表示 (右寄せ)
            String rssiStr = String(info.rssi) + "dBm";
            sprite.setTextDatum(TR_DATUM); // 右上基準 (座標は右上を指定)
            sprite.drawString(rssiStr, sprite.width() - 5, yPos + i * lineHeight);
            sprite.setTextDatum(TL_DATUM); // 左上基準に戻す
        }
        if (networkCount > maxLines) {
             sprite.setCursor(5, sprite.height() - 55); // フッターの上あたり
             sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
             sprite.print("More networks... (Scroll N/A)");
             sprite.setTextColor(TFT_WHITE, TFT_BLACK);
        }
    }
    // ★★★ Button hints 修正 ★★★
    sprite.setTextDatum(BC_DATUM); // 下中央揃え
    sprite.drawString("A:Select(TODO) / B:Use AP / C:Back", sprite.width() / 2, sprite.height() - 10);
    sprite.setTextDatum(TL_DATUM); // 左上基準に戻す
}

// ★★★ APモード設定中の画面表示 (引数変更、内容は同じ) ★★★
void Display::displayAPConfigScreen(APConfigPortal& apPortal) {
    sprite.fillSprite(TFT_DARKCYAN); // 色を変更
    sprite.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    sprite.setTextFont(4); sprite.setTextSize(1);
    sprite.setTextDatum(MC_DATUM); // 中央揃え
    sprite.drawString("AP Config Mode", sprite.width()/2, 20);

    sprite.setTextFont(2); sprite.setTextSize(1);
    sprite.setTextDatum(TL_DATUM); // 左上基準
    sprite.setCursor(10, 50);
    sprite.printf("Connect to WiFi:");
    sprite.setCursor(10, 70);
    sprite.setTextSize(2);
    sprite.print(AP_SETUP_SSID); // config.hpp で定義したSSIDを表示
    sprite.setTextSize(1);

    sprite.setCursor(10, 110);
    sprite.print("Open browser to:");
    sprite.setCursor(10, 130);
    sprite.setTextSize(2);
    sprite.print("http://192.168.4.1"); // M5StackのデフォルトAP IP
    sprite.setTextSize(1);

    // Button hints (APモード中は基本的に操作不要だが、キャンセル用に戻るボタンを表示)
    sprite.setTextDatum(BC_DATUM); // 下中央揃え
    sprite.drawString("Waiting... / C:Cancel", sprite.width() / 2, sprite.height() - 10);
    sprite.setTextDatum(TL_DATUM); // 左上基準に戻す
}