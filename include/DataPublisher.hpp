#ifndef DATA_PUBLISHER_HPP
#define DATA_PUBLISHER_HPP

#include <Arduino.h>
#include <WiFi.h> // WiFi ライブラリ
#include <HTTPClient.h> // HTTPClient ライブラリ
#include "config.hpp"
#include "TrackerData.hpp"
#include "WifiManager.hpp" // WifiManagerクラスの前方宣言またはインクルード

class DataPublisher {
public:
    // コンストラクタ: WifiManagerへの参照を受け取る
    DataPublisher(WifiManager& wifi);
    // 送信先URLを設定するメソッド
    void begin(const String& url, DriveType type);
    // 必要に応じてデータを送信するメソッド
    bool publishIfNeeded(const TrackerData& data);

private:
    WifiManager& wifiManager;       // Wi-Fi接続状態確認用
    String endpointUrl;             // 送信先URL
    unsigned long lastPublishTimeMs; // 最終送信時刻 (送信間隔制御用)
    DriveType drive_type;

    // 埋め込み用の証明書変数は削除済み
};

#endif // DATA_PUBLISHER_HPP