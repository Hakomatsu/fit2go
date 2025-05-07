#include "DataPublisher.hpp"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h> // ★ WiFiClientSecureヘッダー ★
#include <SD.h>              // ★ SDカードアクセス用ヘッダー ★
#include <FS.h>              // ★ ファイルシステムヘッダー ★

// ★ getCurrentTimestampMs 関数のプロトタイプ宣言 (main.cpp で定義) ★
extern uint64_t getCurrentTimestampMs();

// コンストラクタ
DataPublisher::DataPublisher(WifiManager& wifi) :
    wifiManager(wifi), lastPublishTimeMs(0)
{}

// 送信先URLを設定
void DataPublisher::begin(const String& url, DriveType type) {
    drive_type = type;
    endpointUrl = url;
     if (endpointUrl.length() == 0) {
        Serial.println("Warning: Data Publisher initialized with empty URL.");
    } else {
        Serial.printf("Data Publisher initialized with URL: %s\n", endpointUrl.c_str());
    }
}

// 必要に応じてデータを送信
bool DataPublisher::publishIfNeeded(const TrackerData& data) {
    unsigned long currentMillis = millis();

    if (endpointUrl.length() == 0)
        return false;
    if (drive_type == DriveType::TIMER_DRIVEN)
        if (!wifiManager.isConnected() || (currentMillis - lastPublishTimeMs < DATA_PUBLISH_INTERVAL_MS)) {
            // Serial.printf("Wait for publish interval.\n");
            return false;
        }

    bool useHttps = endpointUrl.startsWith("https");
    if (useHttps) {
        Serial.printf("[%lu] Attempting to publish data via HTTPS...\n", currentMillis);
    }
    else {
        Serial.printf("[%lu] Attempting to publish data via HTTP...\n", currentMillis);
    }

    WiFiClient* clientPtr;
    WiFiClientSecure clientSecure;
    WiFiClient clientHttp;

    if (useHttps) {
        File rootCAFile = SD.open(ROOT_CA_PEM_PATH, FILE_READ);
        if (!rootCAFile || rootCAFile.isDirectory()) {
            Serial.printf("Error: Failed to open Root CA file: %s\n", ROOT_CA_PEM_PATH);
            if(rootCAFile) rootCAFile.close();
            return false;
        }
        Serial.printf("Loading Root CA from %s\n", ROOT_CA_PEM_PATH);

        size_t fileSize = rootCAFile.size();
        if (fileSize == 0) {
             Serial.println("Error: Root CA file is empty!");
             rootCAFile.close();
             return false;
        }

        String rootCAData;
        rootCAData.reserve(fileSize);
        rootCAData = rootCAFile.readString();
        rootCAFile.close();

        if (rootCAData.length() > 0) {
             clientSecure.setCACert(rootCAData.c_str());
             Serial.println("Root CA certificate pointer set. Validation occurs during connection.");
        } else {
             Serial.println("Error: Failed to read Root CA file content!");
             return false;
        }
        clientPtr = &clientSecure;
    } else {
        clientPtr = &clientHttp;
    }

    HTTPClient http;
    String serverUrl = endpointUrl;
    bool connectionOpened = false;

    if (useHttps) {
        connectionOpened = http.begin(*clientPtr, serverUrl);
    } else {
        connectionOpened = http.begin(serverUrl);
    }

    if (connectionOpened) {
        http.addHeader("Content-Type", "application/json");

        StaticJsonDocument<1024> doc;
        doc["timestamp_ms"] = getCurrentTimestampMs();
        doc["session_time_s"] = data.sessionElapsedTimeMs / 1000.0;
        doc["session_dist_km"] = data.sessionDistanceKm;
        doc["session_cal_kcal"] = data.sessionCaloriesKcal;
        doc["rpm"] = data.currentRpm;
        doc["speed_kmh"] = data.currentSpeedKmh;
        doc["mets"] = data.currentMets;
        doc["total_time_s"] = (double)data.cumulativeTimeMs / 1000.0;
        doc["total_dist_km"] = data.cumulativeDistanceKm;
        doc["total_cal_kcal"] = data.cumulativeCaloriesKcal;
        uint64_t chipid = ESP.getEfuseMac();
        char chipid_str[18];
        snprintf(chipid_str, sizeof(chipid_str), "%04X%08X", (uint16_t)(chipid>>32), (uint32_t)chipid);
        doc["device_id"] = chipid_str;

        String jsonBuffer;
        serializeJson(doc, jsonBuffer);

        Serial.println("JSON Payload:");
        Serial.println(jsonBuffer);

        int httpCode = http.POST(jsonBuffer);

        if (httpCode > 0) {
            Serial.printf("[HTTP%s] POST... code: %d\n", useHttps ? "S" : "", httpCode);
            if (httpCode >= 200 && httpCode < 300) {
                String payload = http.getString();
                Serial.println("[HTTP] Response:");
                Serial.println(payload);
                lastPublishTimeMs = currentMillis;
                http.end();
                return true;
            } else {
                 String payload = http.getString();
                 Serial.printf("[HTTP%s] POST failed with code %d, Response: %s\n", useHttps ? "S" : "", httpCode, payload.c_str());
            }
        } else {
            Serial.printf("[HTTP%s] POST... failed, error: %s\n", useHttps ? "S" : "", http.errorToString(httpCode).c_str());
        }
        http.end();

    } else {
        Serial.printf("[HTTP%s] Unable to begin connection to %s\n", useHttps ? "S" : "", serverUrl.c_str());
    }

    return false;
}