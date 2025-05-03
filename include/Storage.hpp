#ifndef STORAGE_HPP
#define STORAGE_HPP

#include <Preferences.h>
#include <SD.h>
#include <FS.h>
#include "config.hpp"
#include "TrackerData.hpp"
#include <ArduinoJson.h> // ★ ArduinoJson をインクルード ★
#include <vector>       // ★ vector をインクルード ★
#include <utility>      // ★ pair をインクルード ★

// ★ JSONドキュメント容量定義 ★
#define JSON_CONFIG_CAPACITY 1024    // 設定ファイル用
#define JSON_LATEST_CAPACITY 256     // 最新累積データ用
#define JSON_HISTORY_ENTRY_CAPACITY 256 // 履歴データ(1行分)用

class Storage {
public:
    Storage();
    bool begin(); // SDカードとNVS初期化

    // --- 設定ファイル (JSON) 関連 ---
    bool loadConfigFromJson(); // ★ JSONファイルを読み込みパース ★
    bool getWifiCredential(int index, String& ssid, String& pass); // パース結果からWiFi情報を取得
    String getEndpointUrl(); // パース結果からURLを取得
    int getWifiCredentialCount(); // パース結果のWiFi情報数を取得

    // --- NVS 関連 (WiFi用) ---
    bool loadCredentialsFromNVS(String& ssid, String& pass); // ★ NVSからのみ読み込み ★
    bool saveWiFiCredentialsToNVS(const String& ssid, const String& pass); // ★ NVSへ保存 ★

    // --- 累積データ関連 (SDカード - JSON形式) ---
    bool loadCumulativeDataFromSD(TrackerData& data);     // cumulative_latest.json からロード
    bool saveLatestDataToSD(const TrackerData& data);     // cumulative_latest.json へ保存 (上書き)
    bool appendHistoryDataToSD(const TrackerData& data);  // cumulative_history.jsonl へ追記

    // ★★★ ファイル読み込みヘルパー ★★★
    String readFileContent(const char* path);


private: // ★★★ private セクション ★★★
    Preferences preferences; // WiFi認証情報用NVS
    bool sdCardOk;           // SDカードが利用可能か

    // ★ JSONパース結果保持用 ★
    bool configLoaded; // 設定ファイルがロード・パースされたか
    String endpointUrlFromJson; // JSONから読み込んだURL
    std::vector<std::pair<String, String>> wifiCredentials; // SSIDとPasswordのペアを格納
};

#endif // STORAGE_HPP