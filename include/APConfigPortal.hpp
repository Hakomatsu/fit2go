#ifndef AP_CONFIG_PORTAL_HPP
#define AP_CONFIG_PORTAL_HPP

#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include "Storage.hpp"     // Storageクラスを使うため
#include "WifiManager.hpp" // WifiManagerクラスを使うため (scanNetworks)
#include "config.hpp"

class APConfigPortal {
public:
    // Storage と WifiManager の参照を受け取る
    APConfigPortal(Storage& storageRef, WifiManager& wifiManagerRef);

    bool start();                     // APモードとWebサーバーを開始
    void stop();                      // APモードとWebサーバーを停止
    void handleClient();              // APモード中のクライアント処理 (loopから呼ぶ)
    bool isActive() const;            // APポータルが動作中か
    bool wereCredentialsSaved() const; // 設定が保存されたか (再起動トリガー用)
    void resetCredentialsSavedFlag(); // 保存フラグをリセット

private:
    Storage& storage;             // NVSへの保存用
    WifiManager& wifiManager;     // スキャン機能の利用用
    AsyncWebServer server;        // Webサーバー
    DNSServer dnsServer;          // DNSサーバー
    bool portalActive;            // APポータル動作中フラグ
    bool credentialsSavedFlag;    // 認証情報が保存されたか

    // --- Webサーバーリクエストハンドラ (private) ---
    void handleRootRequest(AsyncWebServerRequest *request);
    void handleScanRequest(AsyncWebServerRequest *request);
    void handleSaveRequest(AsyncWebServerRequest *request);
    void handleNotFound(AsyncWebServerRequest *request);
};

#endif // AP_CONFIG_PORTAL_HPP