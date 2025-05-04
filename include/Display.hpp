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
    void begin(); // ディスプレイとSpriteの初期化
    // ★★★ update の引数を変更 ★★★
    void update(const TrackerData& data, AppState state, WifiManager& wifiManager, APConfigPortal& apPortal);
    void showMessage(const String& msg, int size = 2, bool clear = true); // メッセージ表示用
    void clear(); // 画面クリア用

private:
    TFT_eSprite sprite; // ちらつき防止用Sprite

    // 画面描画用プライベートメソッド
    void displayTrackingScreen(const TrackerData& data, WifiManager& wifiManager); // トラッキング中
    void displayIdleScreen(const TrackerData& data, WifiManager& wifiManager);     // アイドル中
    void displayStoppingScreen(const TrackerData& data, WifiManager& wifiManager); // ★ 追加: 一時停止中 ★
    void displayWifiScreen(WifiManager& wifiManager);           // Wi-Fi設定メニュー
    void displayScanResultsScreen(WifiManager& wifiManager);    // Wi-Fiスキャン結果
    void displayAPConfigScreen(APConfigPortal& apPortal);       // Wi-Fi AP設定モード中

    // ヘルパー関数
    String formatTime(unsigned long ms);                // 時間フォーマット (HH:MM:SS)
    String formatCumulativeTime(uint64_t ms);           // 長時間フォーマット (Xd Yh Zm)
};

#endif // DISPLAY_HPP