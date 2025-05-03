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
const unsigned long TIMER_STOP_DELAY_MS = 3000;
const unsigned long SLEEP_TIMEOUT_MS = 63000;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
const unsigned long DATA_PUBLISH_INTERVAL_MS = 10000;
const unsigned long METRICS_CALC_INTERVAL_MS = 1000;
const uint16_t PCNT_FILTER_VALUE = 1023;
const int16_t PCNT_EVENT_THRESHOLD = 1;

// --- 計算用定数 ---
const float DISTANCE_PER_REV_M = 4.4466f;
const float CALORIES_RPM_K1_FACTOR = 0.00113889f;

// --- 設定ファイルパス (SDカード) ---
extern const char* CONFIG_JSON_PATH;
extern const char* LATEST_DATA_JSON_PATH; // ★ .json に変更 ★
extern const char* HISTORY_DATA_JSONL_PATH; // ★ .jsonl に変更 ★

// --- NVS 設定 (WiFi認証情報用) ---
extern const char* NVS_NAMESPACE;
extern const char* NVS_KEY_WIFI_SSID;
extern const char* NVS_KEY_WIFI_PASS;

// --- APモード設定 ---
extern const char* AP_SETUP_SSID;

// --- NTP 設定 (★ 追加 ★) ---
extern const char* NTP_SERVER1;
extern const char* NTP_SERVER2;
extern const long GMT_OFFSET_SEC; // JST = UTC+9
extern const int DAYLIGHT_OFFSET_SEC; // JST = 0

// --- 状態定義 ---
enum class AppState {
    INITIALIZING,
    IDLE_DISPLAY,
    TRACKING_DISPLAY,
    WIFI_CONNECTING, // NVS/JSONから接続試行中
    WIFI_SCANNING,   // WiFiスキャン結果表示中
    WIFI_SETUP,      // WiFi設定メニュー画面
    WIFI_AP_CONFIG,  // APモードでの設定中
    SLEEPING
};

#endif // CONFIG_HPP