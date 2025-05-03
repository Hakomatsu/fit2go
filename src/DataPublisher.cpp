#include "DataPublisher.hpp"
#include <ArduinoJson.h> // ★ ArduinoJson ライブラリのヘッダ

// コンストラクタでは WifiManager の参照のみ受け取る
DataPublisher::DataPublisher(WifiManager& wifi) :
    wifiManager(wifi), lastPublishTimeMs(0)
{}

// URLをセットアップ時に設定
void DataPublisher::begin(const String& url) {
    endpointUrl = url;
     if (endpointUrl.length() == 0) {
        Serial.println("Warning: Data Publisher initialized with empty URL.");
    } else {
        Serial.printf("Data Publisher initialized with URL: %s\n", endpointUrl.c_str());
    }
}

bool DataPublisher::publishIfNeeded(const TrackerData& data) {
    unsigned long currentMillis = millis();

    // URLが空かチェック
    if (endpointUrl.length() == 0) {
        return false;
    }

    // WiFi接続と送信間隔をチェック
    if (!wifiManager.isConnected() || (currentMillis - lastPublishTimeMs < DATA_PUBLISH_INTERVAL_MS)) {
        return false;
    }

    Serial.printf("[%lu] Attempting to publish data...\n", currentMillis);

    HTTPClient http;
    String serverUrl = endpointUrl;

    if (http.begin(serverUrl)) { // HTTP (HTTPSの場合は WiFiClientSecure を使う)
        http.addHeader("Content-Type", "application/json");

        // ★★★ JSONドキュメントのサイズ指定を追加 ★★★
        StaticJsonDocument<1024> doc; // サイズは必要に応じて調整

        // Add session data
        doc["session_time_s"] = data.sessionElapsedTimeMs / 1000.0;
        doc["session_dist_km"] = data.sessionDistanceKm;
        doc["session_cal_kcal"] = data.sessionCaloriesKcal;

        // Add current data
        doc["rpm"] = data.currentRpm;
        doc["speed_kmh"] = data.currentSpeedKmh;
        doc["mets"] = data.currentMets;

        // Add cumulative data
        doc["total_time_s"] = (double)data.cumulativeTimeMs / 1000.0;
        doc["total_dist_km"] = data.cumulativeDistanceKm;
        doc["total_cal_kcal"] = data.cumulativeCaloriesKcal;

        // Add device identifier (optional)
        uint64_t chipid = ESP.getEfuseMac();
        char chipid_str[18];
        snprintf(chipid_str, sizeof(chipid_str), "%04X%08X", (uint16_t)(chipid>>32), (uint32_t)chipid);
        doc["device_id"] = chipid_str;


        String jsonBuffer;
        serializeJson(doc, jsonBuffer);
        Serial.println("JSON Payload:");
        Serial.println(jsonBuffer);

        // Send POST request
        int httpCode = http.POST(jsonBuffer);

        if (httpCode > 0) {
            Serial.printf("[HTTP] POST... code: %d\n", httpCode);
            if (httpCode >= 200 && httpCode < 300) { // Success range
                String payload = http.getString();
                Serial.println("[HTTP] Response:");
                Serial.println(payload);
                lastPublishTimeMs = currentMillis;
                http.end();
                return true;
            } else {
                 String payload = http.getString();
                 Serial.printf("[HTTP] POST failed with code %d, Response: %s\n", httpCode, payload.c_str());
            }
        } else {
            Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();

    } else {
        Serial.printf("[HTTP] Unable to begin connection to %s\n", serverUrl.c_str());
    }

    return false;
}