#include "config.hpp"

// --- 設定ファイルパス (SDカード) ---
const char* CONFIG_JSON_PATH = "/config.json";
const char* LATEST_DATA_JSON_PATH = "/cumulative_latest.json"; // .json
const char* HISTORY_DATA_JSONL_PATH = "/cumulative_history.jsonl"; // .jsonl
const char* ROOT_CA_PEM_PATH = "/root_ca.pem"; // ★ ルートCAファイルパス定義 ★

// --- NVS 設定 (不揮発メモリ) ---
const char* NVS_NAMESPACE = "tracker";
const char* NVS_KEY_WIFI_SSID = "wifiSSID";
const char* NVS_KEY_WIFI_PASS = "wifiPASS";

// --- APモード設定 ---
const char* AP_SETUP_SSID = "M5Stack_Setup";

// --- NTP 設定 ---
const char* NTP_SERVER1 = "ntp.nict.jp"; // 日本標準時NTP
const char* NTP_SERVER2 = "pool.ntp.org"; // フォールバック
const long GMT_OFFSET_SEC = 9 * 3600;     // JST = UTC+9時間 (秒単位)
const int DAYLIGHT_OFFSET_SEC = 0;        // サマータイムなし

// 他の const 変数は config.hpp 内で定義済み