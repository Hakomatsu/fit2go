#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "driver/pcnt.h"
#include <stdint.h>

// --- ハードウェア設定 ---
const int PULSE_INPUT_PIN = 36;
const pcnt_unit_t PCNT_UNIT = PCNT_UNIT_0;
const pcnt_channel_t PCNT_CHANNEL = PCNT_CHANNEL_0;
const int DEBUG_LED_PIN = 2;

// --- 動作設定 ---
const int PULSES_PER_REVOLUTION = 1;
const unsigned long TIMER_STOP_DELAY_MS = 3000; // 3秒
const unsigned long SLEEP_TIMEOUT_MS = 63000; // 63秒 (TIMER_STOP_DELAY_MS も含む)
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
const unsigned long DATA_PUBLISH_INTERVAL_MS = 10000; // 10秒
const unsigned long METRICS_CALC_INTERVAL_MS = 1000; // 1秒
const uint16_t PCNT_FILTER_VALUE = 1023; // PCNTノイズフィルタ値 (apb_clk cycles)
const int16_t PCNT_EVENT_THRESHOLD = 1;  // PCNTパルスカウントイベントしきい値

// --- 計算用定数 ---
const float DISTANCE_PER_REV_M = 4.4466f; // 1回転あたりの距離 (メートル)
const float CALORIES_RPM_K1_FACTOR = 0.00113889f; // カロリー計算係数 (RPMからkcal/secへ)

// --- 設定ファイルパス (SDカード) ---
extern const char* CONFIG_JSON_PATH;          // Wi-Fi設定、エンドポイントURL用
extern const char* LATEST_DATA_JSON_PATH;   // 最新累積データ用 (.json)
extern const char* HISTORY_DATA_JSONL_PATH; // 履歴データ用 (.jsonl)

// --- NVS 設定 (WiFi認証情報用) ---
extern const char* NVS_NAMESPACE;           // NVS名前空間
extern const char* NVS_KEY_WIFI_SSID;       // NVSキー (SSID)
extern const char* NVS_KEY_WIFI_PASS;       // NVSキー (Password)

// --- APモード設定 ---
extern const char* AP_SETUP_SSID;           // APモード時のSSID

// --- NTP 設定 ---
extern const char* NTP_SERVER1;             // NTPサーバー1
extern const char* NTP_SERVER2;             // NTPサーバー2 (フォールバック)
extern const long GMT_OFFSET_SEC;           // GMTからのオフセット秒 (JST = UTC+9)
extern const int DAYLIGHT_OFFSET_SEC;       // 夏時間オフセット秒 (JST = 0)

// --- 状態定義 ---
enum class AppState {
    INITIALIZING,       // 初期化中
    IDLE_DISPLAY,       // アイドル画面表示中
    TRACKING_DISPLAY,   // トラッキング画面表示中
    STOPPING,           // ★ 追加: トラッキング一時停止中 (タイマー停止後、スリープ前) ★
    WIFI_CONNECTING,    // Wi-Fi接続試行中
    WIFI_SCANNING,      // Wi-Fiスキャン結果表示中
    WIFI_SETUP,         // Wi-Fi設定メニュー画面
    WIFI_AP_CONFIG,     // Wi-Fi APモード設定中
    SLEEPING            // スリープ移行準備中/実行中
};

#endif // CONFIG_HPP