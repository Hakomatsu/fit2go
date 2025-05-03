#include "WifiManager.hpp"
#include <M5Stack.h> // For Serial
// #include <ArduinoJson.h> // 不要

// コンストラクタ
WifiManager::WifiManager(Storage& storage) :
    storage(storage),
    lastStatus(WL_IDLE_STATUS),
    connectAttemptTime(0),
    isConnecting(false),
    scanning(false)
{}

void WifiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);
    Serial.println("WiFi Manager initialized.");
    currentStatus = "WiFi Idle";
    // 起動時の自動接続 (NVS -> YAML[0])
    if (!connect()) {
         // Serial.println("Initial auto-connect failed or no credentials.");
    }
}

// 自動接続 (NVS優先、次にYAMLの最初の設定)
bool WifiManager::connect() {
    if (isConnected()) {
        currentStatus = "Connected: " + WiFi.SSID();
        return true;
    }
    if (isConnecting && (millis() - connectAttemptTime < WIFI_CONNECT_TIMEOUT_MS)) {
         currentStatus = "Connecting to " + currentSSID + "...";
         return false;
    }

    String ssid, pass;
    // まずNVSから試す
    if (storage.loadCredentialsFromNVS(ssid, pass)) {
        if (isConnecting && currentSSID == ssid) {
             currentStatus = "Connecting to " + currentSSID + "... (retrying)";
             return false;
        }
        currentSSID = ssid;
        Serial.printf("Attempting to connect to SSID: %s (from NVS)\n", ssid.c_str());
        currentStatus = "Connecting(NVS) " + ssid + "...";
        isConnecting = true;
        connectAttemptTime = millis();
        WiFi.begin(ssid.c_str(), pass.c_str());
        return false; // 接続試行開始
    } else {
        Serial.println("Not found in NVS, trying YAML[0]...");
        // NVSになければYAMLの最初の設定を試す
        return connectFromYaml(0); // ★ connectFromYaml を呼び出す ★
    }
}

// ★ YAMLの指定indexで接続試行 ★
bool WifiManager::connectFromYaml(int index) {
     if (isConnected()) {
        currentStatus = "Connected: " + WiFi.SSID();
        return true;
    }
     if (isConnecting && (millis() - connectAttemptTime < WIFI_CONNECT_TIMEOUT_MS)) {
         currentStatus = "Connecting to " + currentSSID + "...";
         return false; // 前回の接続試行中
    }

    String ssid, pass;
    // storageのメソッドでYAMLから指定indexの情報を取得
    if (storage.getWifiCredential(index, ssid, pass)) {
         if (isConnecting && currentSSID == ssid) { // 同じSSIDへの再試行は避ける
             currentStatus = "Connecting to " + currentSSID + "... (retrying)";
             return false;
         }
         currentSSID = ssid;
         Serial.printf("Attempting to connect using YAML[%d] to SSID: %s\n", index, ssid.c_str());
         currentStatus = "Connecting(YAML) " + ssid + "...";
         isConnecting = true;
         connectAttemptTime = millis();

         WiFi.begin(ssid.c_str(), pass.c_str());
         // ★ YAMLから読めたらNVSにも保存しておく ★
         storage.saveWiFiCredentialsToNVS(ssid, pass);
         return false; // 接続試行開始

    } else {
        currentStatus = "Could not read WiFi from YAML[" + String(index) + "]";
        Serial.println(currentStatus);
        isConnecting = false;
        return false;
    }
}

void WifiManager::disconnect() {
    WiFi.disconnect(true);
    delay(100);
    currentStatus = "Disconnected.";
    isConnecting = false;
    currentSSID = "";
    Serial.println(currentStatus);
}

bool WifiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool WifiManager::isAttemptingConnection() const {
     return isConnecting;
}

void WifiManager::updateStatus() {
    wl_status_t current_wl_status = WiFi.status();
    if (isConnecting) {
        if (current_wl_status == WL_CONNECTED) {
            Serial.println("\nWiFi connected!");
            Serial.print("IP address: "); Serial.println(WiFi.localIP());
            currentStatus = "Connected: " + WiFi.SSID() + "\nIP: " + WiFi.localIP().toString();
            isConnecting = false;
        } else if (millis() - connectAttemptTime > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("\nConnection Timeout.");
            currentStatus = "Timeout connecting to " + currentSSID;
            WiFi.disconnect(true); // タイムアウトしたら切断
            isConnecting = false;
            currentSSID = ""; // 試行中SSIDクリア
        } else {
             // isConnecting が true の間は Connecting... メッセージを維持
             currentStatus = "Connecting to " + currentSSID + "...";
        }
    } else { // isConnecting == false
        if (current_wl_status != lastStatus) {
             if (current_wl_status == WL_CONNECTED && lastStatus != WL_CONNECTED) {
                  currentStatus = "Connected: " + WiFi.SSID() + "\nIP: " + WiFi.localIP().toString();
                  Serial.println("WiFi (re)connected.");
             } else if (current_wl_status != WL_CONNECTED && lastStatus == WL_CONNECTED) {
                  currentStatus = "Connection Lost.";
                  Serial.println(currentStatus);
             } else if (current_wl_status == WL_NO_SSID_AVAIL || current_wl_status == WL_CONNECT_FAILED){
                  currentStatus = "Connect Failed";
                  Serial.println(currentStatus);
             } else if (current_wl_status == WL_DISCONNECTED || current_wl_status == WL_IDLE_STATUS) {
                 // 最後に接続されていた状態から切断された場合のみメッセージ変更
                 if (lastStatus == WL_CONNECTED) {
                     currentStatus = "Disconnected.";
                     Serial.println(currentStatus);
                 } else if (lastStatus != WL_DISCONNECTED && lastStatus != WL_IDLE_STATUS) {
                     // 接続中でもなく、最後に接続されていたわけでもなければ Idle
                     currentStatus = "WiFi Idle";
                 }
             }
        }
         // デフォルトの状態表示を更新 (タイムアウトや失敗メッセージがない場合)
         if (!isConnected() && !isConnecting && !scanning &&
             currentStatus.indexOf("Timeout") == -1 && currentStatus.indexOf("Failed") == -1 &&
             currentStatus.indexOf("No WiFi") == -1 && currentStatus.indexOf("Lost") == -1 &&
             currentStatus.indexOf("read") == -1 && currentStatus.indexOf("Scanning") == -1 &&
             currentStatus.indexOf("Disconnected") == -1) // Disconnectedメッセージも上書きしない
         {
                currentStatus = "WiFi Idle";
         }
    }
    // Update lastStatus regardless of change if not connecting/scanning
    if (!isConnecting && !scanning) {
         lastStatus = current_wl_status;
    }
}

IPAddress WifiManager::getLocalIP() {
    return WiFi.localIP();
}


// ★★★ Wi-Fiスキャン関連メソッドの実装 (変更なし) ★★★
int WifiManager::scanNetworks() {
    if (scanning) { return -1; }
    Serial.println("Starting WiFi Scan...");
    currentStatus = "Scanning...";
    scanning = true;
    scanResults.clear();
    int n = WiFi.scanNetworks(false, true); // 非同期=false, ShowHidden=true
    scanning = false;
    lastScanTime = millis();
    if (n < 0) { Serial.printf("WiFi Scan failed! Error code: %d\n", n); currentStatus = "Scan failed"; }
    else if (n == 0) { Serial.println("No networks found"); currentStatus = "No networks found"; }
    else {
        Serial.printf("%d networks found:\n", n);
        currentStatus = String(n) + " networks found";
        scanResults.reserve(n); // メモリ確保
        for (int i = 0; i < n; ++i) {
            WiFiScanInfo info;
            info.ssid = WiFi.SSID(i); info.rssi = WiFi.RSSI(i); info.encryptionType = WiFi.encryptionType(i);
            scanResults.push_back(info);
            Serial.printf("  %d: %s (%d dBm) %s\n", i + 1, info.ssid.c_str(), info.rssi, (info.encryptionType == WIFI_AUTH_OPEN) ? " " : "*");
        }
        // SSIDでソートなどしても良いかも
    }
    return n;
}

int WifiManager::getScanResultCount() const { return scanResults.size(); }

WiFiScanInfo WifiManager::getScanResult(int index) const {
    if (index >= 0 && index < scanResults.size()) { return scanResults[index]; }
    else { return WiFiScanInfo{"Index Err", 0, WIFI_AUTH_OPEN}; }
}

// getStatusMessage (APモード部分削除済み)
String WifiManager::getStatusMessage() {
    if (scanning) { return "Scanning..."; }
    else if (isConnecting) { return "Connecting to " + currentSSID + "..."; }
    return currentStatus;
}

// --- APモード関連メソッドはすべて削除 ---