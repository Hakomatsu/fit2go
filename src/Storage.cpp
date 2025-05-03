#include "Storage.hpp"
#include <M5Stack.h> // Serial用

// ★ getCurrentTimestampMs 関数のプロトタイプ宣言 (main.cpp で定義) ★
// これにより、Storage.cpp 内からこの関数を呼び出せるようになる
extern uint64_t getCurrentTimestampMs();

Storage::Storage() : sdCardOk(false), configLoaded(false) {}

// begin
bool Storage::begin() {
    bool nvs_ok = preferences.begin(NVS_NAMESPACE, false);
    if (!nvs_ok) {
        Serial.printf("Warning: Could not initialize NVS Namespace '%s' for R/W.\n", NVS_NAMESPACE);
    } else {
        Serial.printf("NVS Initialized OK. Namespace: %s\n", NVS_NAMESPACE);
        preferences.end();
    }
    sdCardOk = SD.begin(TFCARD_CS_PIN, SPI, 40000000);
    if (!sdCardOk) {
        Serial.println("SD Card Mount Failed!");
    } else {
        Serial.println("SD Card Mounted.");
        loadConfigFromJson(); // JSON設定ファイルのロード
    }
    return sdCardOk;
}

// readFileContent
String Storage::readFileContent(const char* path) {
    String content = "";
    if (!sdCardOk) {
        Serial.printf("[readFileContent] SD Card not ready, cannot read %s\n", path);
        return content;
    }
    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if(file) file.close();
        Serial.printf("Warning: Failed to open file for reading or is dir: %s\n", path);
        return content;
    }
    if (file.available()) {
        // ファイルサイズチェック (JSON設定ファイル用のみ)
        if (strcmp(path, CONFIG_JSON_PATH) == 0) {
             size_t fileSize = file.size();
             if (fileSize > JSON_CONFIG_CAPACITY * 1.5) {
                 Serial.printf("Warning: Config file %s size (%d bytes) might be too large for JSON capacity (%d).\n", path, fileSize, JSON_CONFIG_CAPACITY);
             }
        }
        content = file.readString();
    } else {
         Serial.printf("Warning: File is empty: %s\n", path);
    }
    file.close();
    return content;
}


// --- JSON 設定ファイル関連 (ArduinoJsonを使用) ---

// JSONファイルを読み込み、パースして結果をメンバー変数に格納
bool Storage::loadConfigFromJson() {
    configLoaded = false;
    endpointUrlFromJson = "";
    wifiCredentials.clear();

    if (!sdCardOk) return false;

    Serial.printf("Loading config from %s (JSON parse)...\n", CONFIG_JSON_PATH);
    String jsonContent = readFileContent(CONFIG_JSON_PATH);
    if (jsonContent.length() == 0) {
        Serial.println("Config file not found or empty.");
        return false;
    }

    // JSONドキュメントオブジェクトを生成
    StaticJsonDocument<JSON_CONFIG_CAPACITY> doc;

    // JSONをパース
    DeserializationError error = deserializeJson(doc, jsonContent);

    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return false;
    }

    // --- データの抽出 ---

    // エンドポイントURL
    if (doc["endpoint_url"].is<const char*>()) {
        endpointUrlFromJson = doc["endpoint_url"].as<String>();
        Serial.printf("Endpoint URL from JSON: %s\n", endpointUrlFromJson.c_str());
    } else {
        Serial.println("Warning: 'endpoint_url' not found or not a string in JSON.");
    }

    // ネットワーク情報 (配列)
    if (doc["networks"].is<JsonArray>()) {
        JsonArray networks = doc["networks"].as<JsonArray>();
        Serial.printf("Found %d network entries in JSON.\n", networks.size());

        // 配列内の各オブジェクトを処理
        for (JsonObject network : networks) {
            if (network && network["ssid"].is<const char*>()) {
                String ssid = network["ssid"].as<String>();
                String pass = ""; // デフォルトは空パスワード

                // password が null でなく、文字列であれば取得
                if (!network["password"].isNull() && network["password"].is<const char*>()) {
                    pass = network["password"].as<String>();
                }

                if (ssid.length() > 0) {
                    wifiCredentials.push_back(std::make_pair(ssid, pass));
                    Serial.printf("Loaded network: %s\n", ssid.c_str());
                }
            } else {
                Serial.println("Warning: Invalid network entry format in JSON.");
            }
        }
    } else {
        Serial.println("Warning: 'networks' key not found or not an Array in JSON.");
    }

    if (endpointUrlFromJson.length() > 0 || wifiCredentials.size() > 0) {
         Serial.println("JSON parsing finished.");
         configLoaded = true;
         return true;
    } else {
         Serial.println("JSON parsing finished, but no valid data found.");
         return false;
    }
}

// JSONパース結果から指定indexのWiFi情報を取得
bool Storage::getWifiCredential(int index, String& ssid, String& pass) {
    // configLoaded フラグをチェックする代わりに、wifiCredentialsが空でないかで判断しても良い
    // if (!configLoaded || index < 0 || index >= wifiCredentials.size()) {
    if (index < 0 || index >= wifiCredentials.size()) {
        return false;
    }
    ssid = wifiCredentials[index].first;
    pass = wifiCredentials[index].second;
    return true;
}

// JSONパース結果からURLを取得
String Storage::getEndpointUrl() {
    // begin()でロード試行済みのはずなので、ロード状態を再チェックしない
    return endpointUrlFromJson;
}

// JSONパース結果のWiFi情報数を取得
int Storage::getWifiCredentialCount() {
     return wifiCredentials.size();
}


// --- NVS 関連 (WiFi用) ---
bool Storage::loadCredentialsFromNVS(String& ssid, String& pass) {
    if (!preferences.begin(NVS_NAMESPACE, true)) {
        Serial.println("[loadCredNVS] NVS begin (readOnly) failed.");
        return false;
    }
    ssid = preferences.getString(NVS_KEY_WIFI_SSID, "");
    pass = preferences.getString(NVS_KEY_WIFI_PASS, "");
    preferences.end();
    if (ssid.length() > 0) {
        Serial.println("WiFi credentials loaded from NVS.");
        return true;
    }
    return false;
}
bool Storage::saveWiFiCredentialsToNVS(const String& ssid, const String& pass) {
     if (!preferences.begin(NVS_NAMESPACE, false)) {
         Serial.println("[saveCredNVS] NVS begin (readWrite) failed.");
         return false;
     }
     bool s1 = preferences.putString(NVS_KEY_WIFI_SSID, ssid);
     bool s2 = preferences.putString(NVS_KEY_WIFI_PASS, pass);
     preferences.end();
     if (s1 && s2) { Serial.println("WiFi credentials saved to NVS."); return true; }
     else { Serial.println("Failed to save WiFi credentials to NVS."); return false; }
}

// --- 累積データ関連 (SDカード - JSON形式) ---

// cumulative_latest.json からデータを読み込む
bool Storage::loadCumulativeDataFromSD(TrackerData& data) {
    // デフォルト値を設定
    data.cumulativeTimeMs = 0;
    data.cumulativeDistanceKm = 0.0f;
    data.cumulativeCaloriesKcal = 0.0f;

    if (!sdCardOk) {
        Serial.println("[LoadLatestSD] SD Card not available.");
        return false;
    }

    Serial.printf("[LoadLatestSD] Reading latest data from: %s\n", LATEST_DATA_JSON_PATH);
    String jsonContent = readFileContent(LATEST_DATA_JSON_PATH);
    if (jsonContent.length() == 0) {
        Serial.println("[LoadLatestSD] File not found or empty. Using default zero values.");
        return false; // ファイルがない場合はデフォルト値 (読み込み失敗ではない)
    }

    StaticJsonDocument<JSON_LATEST_CAPACITY> doc;
    DeserializationError error = deserializeJson(doc, jsonContent);

    if (error) {
        Serial.print("[LoadLatestSD] deserializeJson() failed: ");
        Serial.println(error.c_str());
        Serial.println("[LoadLatestSD] Using default zero values.");
        return false; // パース失敗時はデフォルト値
    }

    // JSONから値を取得 (キーが存在しない場合はデフォルト値0が使われる)
    // ★ uint64_t の取得には注意が必要。long long もしくは double で受ける ★
    if (doc["time_ms"].is<unsigned long long>()) {
         data.cumulativeTimeMs = doc["time_ms"].as<unsigned long long>();
    } else if (doc["time_ms"].is<double>()) { // 古い形式(float)も考慮
         data.cumulativeTimeMs = (uint64_t)doc["time_ms"].as<double>();
    } else {
         data.cumulativeTimeMs = 0; // 不正な型なら0
         if(doc["time_ms"].is<const char*>() || doc["time_ms"].is<int>()){ // 他の型の可能性も考慮してログ
              Serial.println("[LoadLatestSD] Warning: time_ms has unexpected type.");
         }
    }

    data.cumulativeDistanceKm = doc["dist_km"] | 0.0f; // float
    data.cumulativeCaloriesKcal = doc["cal_kcal"] | 0.0f; // float

    Serial.printf("[LoadLatestSD] Parsed data: Time=%llu ms, Dist=%.4f km, Cal=%.2f kcal\n",
                   data.cumulativeTimeMs, data.cumulativeDistanceKm, data.cumulativeCaloriesKcal);

    return true; // 読み込み成功
}

// cumulative_latest.json へデータを保存 (上書き)
bool Storage::saveLatestDataToSD(const TrackerData& data) {
     if (!sdCardOk) {
        Serial.println("[SaveLatestSD] SD Card not available.");
        return false;
    }

    // Serial.printf("[SaveLatestSD] Saving latest data to: %s\n", LATEST_DATA_JSON_PATH);
    StaticJsonDocument<JSON_LATEST_CAPACITY> doc;

    // JSONオブジェクトにデータを設定
    // ★ uint64_t はそのままではシリアライズできない場合があるので注意 ★
    // ArduinoJson v6 では unsigned long long が使えるはず
    doc["time_ms"] = (unsigned long long)data.cumulativeTimeMs;
    doc["dist_km"] = data.cumulativeDistanceKm; // float はそのまま
    doc["cal_kcal"] = data.cumulativeCaloriesKcal; // float はそのまま

    String outputBuffer;
    // ★ シリアライズサイズをチェック（オプション）★
    // size_t expectedSize = measureJson(doc);
    // if (expectedSize > outputBuffer.capacity()) { /* Handle buffer too small */ }
    serializeJson(doc, outputBuffer);

    // ファイルを開いて書き込み (上書き)
    File file = SD.open(LATEST_DATA_JSON_PATH, FILE_WRITE);
    if (!file) {
        Serial.printf("[SaveLatestSD] Failed to open '%s' for writing.\n", LATEST_DATA_JSON_PATH);
        return false;
    }

    size_t written = file.print(outputBuffer);
    file.close();

    if (written == outputBuffer.length()) {
         // Serial.printf("[SaveLatestSD] Data saved successfully (%d bytes written).\n", written);
        return true;
    } else {
         Serial.println("[SaveLatestSD] File write failed (written bytes mismatch).");
        return false;
    }
}

// ★ cumulative_history.jsonl へデータを追記 (タイムスタンプ取得方法を変更) ★
bool Storage::appendHistoryDataToSD(const TrackerData& data) {
    if (!sdCardOk) {
        Serial.println("[AppendHistSD] SD Card not available.");
        return false;
    }

    // Serial.printf("[AppendHistSD] Appending history data to: %s\n", HISTORY_DATA_JSONL_PATH);

    // 追記する1行分のJSONオブジェクトを作成
    StaticJsonDocument<JSON_HISTORY_ENTRY_CAPACITY> entryDoc;

    // ★★★ getCurrentTimestampMs() でタイムスタンプを取得 ★★★
    uint64_t timestampMs = getCurrentTimestampMs();
    entryDoc["timestamp_ms"] = timestampMs; // ★ キー名を変更し、取得した値を使用 ★

    entryDoc["time_ms"] = (unsigned long long)data.cumulativeTimeMs;
    entryDoc["dist_km"] = data.cumulativeDistanceKm;
    entryDoc["cal_kcal"] = data.cumulativeCaloriesKcal;

    String entryBuffer;
    serializeJson(entryDoc, entryBuffer);

    // ファイルを追記モードで開く
    File file = SD.open(HISTORY_DATA_JSONL_PATH, FILE_APPEND);
    if (!file) {
        Serial.printf("[AppendHistSD] Failed to open '%s' for appending.\n", HISTORY_DATA_JSONL_PATH);
        return false;
    }

    // JSON文字列と改行を書き込む
    size_t written = file.println(entryBuffer); // println で改行を追加
    file.close();

    // println は \r\n または \n を追加するため、+1 or +2 でチェック
    if (written == (entryBuffer.length() + 1) || written == (entryBuffer.length() + 2)) {
         // Serial.printf("[AppendHistSD] History data appended successfully (%d bytes written).\n", written);
        return true;
    } else {
         Serial.printf("[AppendHistSD] File append failed (written bytes: %d, expected: ~%d).\n", written, entryBuffer.length()+1);
        return false;
    }
}