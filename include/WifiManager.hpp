#ifndef WIFI_MANAGER_HPP
#define WIFI_MANAGER_HPP

#include <WiFi.h>
#include "Storage.hpp"
#include "config.hpp"
#include <vector> // ★ vector を使うためにインクルード ★
// #include <ESPAsyncWebServer.h> // 削除
// #include <DNSServer.h>         // 削除

// ★ スキャン結果を保持する構造体 ★
struct WiFiScanInfo {
    String ssid;
    int32_t rssi;
    wifi_auth_mode_t encryptionType;
};

class WifiManager {
public:
    WifiManager(Storage& storage);
    void begin();
    bool connect(); // 自動接続 (NVS優先、次にYAMLの最初の設定で接続試行)
    bool connectFromYaml(int index = 0); // ★ YAMLの指定indexで接続試行 ★
    void disconnect();
    bool isConnected();
    String getStatusMessage(); // 現在の状態メッセージ取得
    void updateStatus(); // 接続状態などを更新
    IPAddress getLocalIP();
    bool isAttemptingConnection() const; // 接続試行中か

    // ★★★ Wi-Fiスキャン関連メソッド ★★★
    int scanNetworks(); // スキャン実行
    int getScanResultCount() const; // スキャン結果数を取得
    WiFiScanInfo getScanResult(int index) const; // 個別のスキャン結果を取得

    // --- APモード関連メソッドは削除 ---

private:
    Storage& storage;
    String currentSSID; // 現在接続中または接続試行中のSSID
    String currentStatus; // 表示用のステータスメッセージ
    wl_status_t lastStatus; // 前回のWiFiステータス
    unsigned long connectAttemptTime; // 接続試行開始時刻
    bool isConnecting; // 現在接続試行中か

    // ★★★ スキャン結果を保持するメンバ変数 ★★★
    std::vector<WiFiScanInfo> scanResults; // スキャン結果リスト
    unsigned long lastScanTime = 0; // 最終スキャン時刻（連続スキャン防止用）
    bool scanning = false; // スキャン実行中フラグ

    // --- APモード関連メンバーは削除 ---
};

#endif // WIFI_MANAGER_HPP