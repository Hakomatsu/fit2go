#ifndef DATA_PUBLISHER_HPP
#define DATA_PUBLISHER_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.hpp"
#include "TrackerData.hpp"
#include "WifiManager.hpp"

class DataPublisher {
public:
    DataPublisher(WifiManager& wifi);
    // URLを後から設定するためのメソッド
    void begin(const String& url);
    // 必要に応じてデータを送信する
    bool publishIfNeeded(const TrackerData& data);

private:
    WifiManager& wifiManager;
    String endpointUrl; // URLを保持
    unsigned long lastPublishTimeMs; // 送信間隔制御
};

#endif // DATA_PUBLISHER_HPP