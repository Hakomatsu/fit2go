#include "APConfigPortal.hpp"
#include <M5Stack.h>
#include <ArduinoJson.h> // JSON用

// コンストラクタ
APConfigPortal::APConfigPortal(Storage& storageRef, WifiManager& wifiManagerRef) :
    storage(storageRef),
    wifiManager(wifiManagerRef), // ★ WifiManagerの参照を保持 ★
    server(80),
    portalActive(false),
    credentialsSavedFlag(false)
{}

// APモード開始
bool APConfigPortal::start() {
    if (portalActive) return true;

    Serial.println("[AP Portal] Starting...");
    // WiFiを切断しAPモードへ
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    delay(100);

    if (!WiFi.softAP(AP_SETUP_SSID)) {
        Serial.println("[AP Portal] Failed to start Soft AP!");
        return false;
    }
    IPAddress apIP = WiFi.softAPIP();
    Serial.print("[AP Portal] AP IP address: "); Serial.println(apIP);

    // DNSサーバー開始
    if (!dnsServer.start(53, "*", apIP)) {
        Serial.println("[AP Portal] Failed to start DNS Server!");
    } else {
        Serial.println("[AP Portal] DNS Server started.");
    }

    // Webサーバーハンドラ設定 (★ /scan を削除 ★)
    server.on("/", HTTP_GET, std::bind(&APConfigPortal::handleRootRequest, this, std::placeholders::_1));
    server.on("/save", HTTP_POST, std::bind(&APConfigPortal::handleSaveRequest, this, std::placeholders::_1));
    server.onNotFound(std::bind(&APConfigPortal::handleNotFound, this, std::placeholders::_1));

    server.begin(); // Webサーバー開始
    Serial.println("[AP Portal] Web server started.");

    portalActive = true;
    credentialsSavedFlag = false;
    return true;
}

// APモード停止
void APConfigPortal::stop() {
    if (!portalActive) return;

    Serial.println("[AP Portal] Stopping...");
    server.end();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA); // STAモードに戻す
    portalActive = false;
}

// APモード中のクライアント処理
void APConfigPortal::handleClient() {
    if (portalActive) {
        dnsServer.processNextRequest();
        // AsyncWebServerは内部処理なので明示的な呼び出し不要
    }
}

// APモードがアクティブか
bool APConfigPortal::isActive() const {
    return portalActive;
}

// 設定が保存されたか
bool APConfigPortal::wereCredentialsSaved() const {
    return credentialsSavedFlag;
}
// 保存フラグのリセット
void APConfigPortal::resetCredentialsSavedFlag(){
     credentialsSavedFlag = false;
}

// --- Webハンドラ ---

// ★★★ ルート ("/") ハンドラ修正: スキャン結果をHTMLに埋め込む ★★★
void APConfigPortal::handleRootRequest(AsyncWebServerRequest *request) {
    Serial.println("[AP Portal] Serving root page with embedded scan results...");

    // WifiManagerから最新のスキャン結果を取得
    String scanResultHtml = "Networks Found:<br>";
    int n = wifiManager.getScanResultCount();
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            WiFiScanInfo info = wifiManager.getScanResult(i);
            // Escape single quotes in SSID for the onclick handler
            String escapedSsid = info.ssid;
            escapedSsid.replace("'", "\\'");
            escapedSsid.replace("\"", "&quot;"); // Also escape double quotes

            scanResultHtml += "<div class=\"network\" onclick=\"document.getElementById('ssid').value='" + escapedSsid + "'; document.getElementById('pass').value='';\">"; // ★ パスワードもクリア ★
            scanResultHtml += info.ssid + " (" + String(info.rssi) + "dBm) " + (info.encryptionType == WIFI_AUTH_OPEN ? "" : "*");
            scanResultHtml += "</div>";
        }
    } else {
        scanResultHtml += "No networks found. (Scan on M5Stack first if needed)";
    }

    // ベースとなるHTML (ファイルから読む代わりにここで定義、ScanボタンとJS削除)
    // SDカードから読む場合は storage.readFileContent(WIFI_CONFIG_HTML_PATH) を使う
    String htmlBase = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>M5Stack WiFi Setup</title>
<style>body{font-family:sans-serif;background-color:#f4f4f4;color:#333;margin:15px;}h1{text-align:center;color:#007bff;}
#scanResults{margin-top:15px;max-height:200px;overflow-y:auto;border:1px solid #ccc;padding:10px;background-color:#fff;}
.network{padding:5px;border-bottom:1px solid #eee;cursor:pointer;}.network:hover{background-color:#e9e9e9;}
label{display:block;margin-top:10px;font-weight:bold;}input[type=text],input[type=password]{width:calc(100% - 12px);padding:5px;margin-top:5px;border:1px solid #ccc;}
button{padding:10px 15px;background-color:#007bff;color:#fff;border:none;border-radius:4px;cursor:pointer;font-size:1em;margin-top:15px;width:100%;}
button:hover{background-color:#0056b3;}
</style></head><body><h1>WiFi Configuration</h1>
<div id="scanResults">
)rawliteral"; // ここまでが前半

    htmlBase += scanResultHtml; // スキャン結果を挿入

    htmlBase += R"rawliteral(
</div>
<form method="POST" action="/save">
<label for="ssid">Network Name (SSID):</label><input type="text" id="ssid" name="ssid" required>
<label for="pass">Password:</label><input type="password" id="pass" name="pass">
<button type="submit">Save & Connect</button>
</form>
</body></html>
)rawliteral"; // ここからが後半

    // ファイル読み込みを優先する場合:
    // String htmlContent = storage.readFileContent(WIFI_CONFIG_HTML_PATH);
    // if (htmlContent.length() > 0) {
    //    // TODO: htmlContent 内のプレースホルダを scanResultHtml で置き換える処理が必要
    //    request->send(200, "text/html", htmlContent);
    // } else {
    //     Serial.println("[AP Portal] Error: Failed to read wifi_config.html from SD card! Using fallback HTML.");
    //     request->send(200, "text/html", htmlBase); // フォールバックとして埋め込みHTMLを使用
    // }
    // 現状は埋め込みHTMLを直接使用
    request->send(200, "text/html", htmlBase);

}


// ★★★ スキャン ("/scan") ハンドラは不要なのでコメントアウトまたは削除 ★★★
/*
void APConfigPortal::handleScanRequest(AsyncWebServerRequest *request) {
    // ... (この関数はもう使わない) ...
}
*/

// 保存 ("/save") ハンドラ
void APConfigPortal::handleSaveRequest(AsyncWebServerRequest *request) {
    Serial.println("[AP Portal] Handling /save request...");
    String ssid = "", pass = "";
    if (request->hasParam("ssid", true)) ssid = request->getParam("ssid", true)->value();
    if (request->hasParam("pass", true)) pass = request->getParam("pass", true)->value();

    if (ssid.length() > 0) {
        Serial.println("[AP Portal] Received SSID: " + ssid);
        // ★ storage のメソッドでNVSに保存 ★
        if (storage.saveWiFiCredentialsToNVS(ssid, pass)) {
            Serial.println("[AP Portal] Credentials saved to NVS successfully.");
            credentialsSavedFlag = true; // 保存成功フラグを立てる
            String html = "<html><body><h1>Configuration Saved!</h1><p>The device will restart shortly.</p></body></html>";
            request->send(200, "text/html", html);
            // 再起動は main loop 側でフラグを見て行う
        } else {
            Serial.println("[AP Portal] Failed to save credentials to NVS!");
            request->send(500, "text/plain", "Error saving configuration (NVS Error).");
        }
    } else {
        request->send(400, "text/plain", "SSID cannot be empty.");
    }
}

// NotFound ハンドラ
void APConfigPortal::handleNotFound(AsyncWebServerRequest *request) {
     // WifiManager.cpp から移動したコードと同じ
     Serial.printf("[AP Portal] NotFound: http://%s%s\n", request->host().c_str(), request->url().c_str());
     if (!request->host().startsWith("192.168.4.1")) {
          request->redirect("/");
     } else {
          request->send(404, "text/plain", "Not found");
     }
}