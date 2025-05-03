#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include <M5Stack.h>
#include "TrackerData.hpp"
#include "config.hpp"

class WifiManager;    // 前方宣言
class APConfigPortal; // ★ APConfigPortal の前方宣言を追加 ★

class Display {
public:
    Display();
    void begin();
    // ★★★ update の引数を変更 ★★★
    void update(const TrackerData& data, AppState state, WifiManager& wifiManager, APConfigPortal& apPortal);
    void showMessage(const String& msg, int size = 2, bool clear = true);
    void clear();

private:
    TFT_eSprite sprite; // ちらつき防止用Sprite

    // 画面描画用プライベートメソッド
    void displayTrackingScreen(const TrackerData& data, WifiManager& wifiManager);
    void displayIdleScreen(const TrackerData& data, WifiManager& wifiManager);
    void displayWifiScreen(WifiManager& wifiManager); // ★ 引数変更 ★
    void displayScanResultsScreen(WifiManager& wifiManager);
    void displayAPConfigScreen(APConfigPortal& apPortal); // ★ 引数変更 ★

    // ヘルパー関数 (変更なし)
    String formatTime(unsigned long ms);
    String formatCumulativeTime(uint64_t ms);
};

#endif // DISPLAY_HPP