#include "web_tune.h"

WebConfig webConfig;

// ==================== 辅助验证函数 ====================

bool WebConfig::validateWiFiPassword(const String& pwd) {
    if (pwd.length() == 0) return true; // 允许开放网络
    if (pwd.length() < 8 || pwd.length() > 63) return false;
    for (size_t i = 0; i < pwd.length(); i++) {
        if (pwd.charAt(i) < 32 || pwd.charAt(i) > 126) return false;//32 是 空格 (Space) 的编码，它是 ASCII 中第一个可打印字符。 
    }                                                               //126 是 波浪号 (~) 的编码，它是 ASCII 中最后一个可打印字符。
    return true;
}

bool WebConfig::validateBaudRate(const String& baud) {
    int val = baud.toInt();
    return val >= 1200 && val <= 2000000;
}

bool WebConfig::validateFlushParams(int minTime, int thresTime, int minSize, int thresSize) {
    if (minTime > thresTime) return false;
    if (minSize > thresSize) return false;
    if (thresSize == 0) return false;
    return true;
}

// JSON 字符串转义，防止 SSID/密码中的 " 或 \ 破坏 JSON
String WebConfig::jsonEscape(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s.charAt(i);
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

// ==================== 核心逻辑 ====================

void WebConfig::begin() {
    DEBUG_PRINTLN("[WebConfig] Starting...");
    
    // 1. 挂载文件系统
    if (!SPIFFS.begin(true)) {
        DEBUG_PRINTLN("[WebConfig] SPIFFS Mount Failed!");
        return;
    }
    DEBUG_PRINTLN("[WebConfig] SPIFFS Mounted.");
    
    //获取MAC地址，后续配对和显示用
    getLocalMacs();
    
    // 2. 启动网络
    setupNetwork();
    
    // 3. 设置路由
    setupRoutes();
    
    // 4. 启动 Web 服务器
    server.begin();
    
    // 5. 启动 DNS (强制门户核心) — 拦截所有域名指向 AP IP
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    // 6. 启动 mDNS — STA 模式下可通过 esp-air.local 访问
    // if (MDNS.begin("esp-air")) {
    //     MDNS.addService("http", "tcp", 80);
    //     DEBUG_PRINTLN("[WebConfig] mDNS started: esp-air.local");
    // } else {
    //     DEBUG_PRINTLN("[WebConfig] mDNS failed!");
    // }
    
    active = true;
    DEBUG_PRINTLN("[WebConfig] HTTP Server Started on port 80");
}

void WebConfig::update() {
    // 1. 处理 DNS 和 Web 请求
    dnsServer.processNextRequest();
    server.handleClient();
    
    // 2. 实时监控 STA 连接状态
    static bool lastConnected = false;
    bool staConnected = (WiFi.status() == WL_CONNECTED);
    bool apHasClient = (WiFi.softAPgetStationNum() > 0);
    bool currConnected = (staConnected || apHasClient);
    
    if (currConnected != lastConnected) {
        lastConnected = currConnected;
        deviceConnected = currConnected;
        
        if (currConnected) {
            DEBUG_PRINTLN("[WiFi] Connected!");
            if (staConnected) {
                DEBUG_PRINTF("[WiFi] STA IP: %s\n", WiFi.localIP().toString().c_str());
            }
            if (apHasClient) {
                DEBUG_PRINTF("[WiFi] AP Clients: %d\n", WiFi.softAPgetStationNum());
            }
        } else {
            DEBUG_PRINTLN("[WiFi] Disconnected / Connecting...");
        }
    }
}

void WebConfig::setupNetwork() {
    WiFi.mode(WIFI_AP_STA);
    
    // === 配置 AP ===
    IPAddress apIP(AP_IP_COMMA);
    IPAddress gateway(AP_IP_COMMA);
    IPAddress subnet(AP_SUBNET_COMMA);
    
    WiFi.softAPConfig(apIP, gateway, subnet);
    WiFi.softAP(apWebCfg_fixed.SSID.c_str(), apWebCfg_fixed.PassWord.c_str());
    
    DEBUG_PRINTF("[WebConfig] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    
    // === 尝试连接 STA ===
    if (HOME_WIFI_AUTO) {
        DEBUG_PRINTF("[WebConfig] Auto Connect Enabled. Target: %s\n", HOME_WIFI_SSID.c_str());
        WiFi.begin(HOME_WIFI_SSID.c_str(), HOME_WIFI_PWD.c_str());
    }
}

// ==================== 路由处理 ====================

void WebConfig::setupRoutes() {
    // 1. 主页
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    
    // 2. 赛博虚空 (OTA 备用页)
    server.on("/cyber-void", HTTP_GET, [this]() { handleCyberVoid(); });
    
    // 3. 获取配置
    server.on("/get-config", HTTP_GET, [this]() { handleGetConfig(); });
    
    // 4. 保存配置 (不重启，保存到NVS后重读)
    server.on("/save-config", HTTP_POST, [this]() { handleSaveConfig(); });
    
    // 5. OTA 处理 (不自动重启，用户拨动按钮退出时重启)
    server.on("/update", HTTP_POST,
        [this]() {
            bool success = !Update.hasError();
            if (success) {
                server.send(200, "text/plain", "OTA OK! 请拨动按钮退出WEB调参以重启生效。");
            } else {
                server.send(200, "text/plain", "OTA FAIL");
            }
            // 不自动重启，等用户拨动按钮
        },
        [this]() {
            HTTPUpload& upload = server.upload();
            
            if (upload.status == UPLOAD_FILE_START) {
                String filename = upload.filename;
                DEBUG_PRINTF("[OTA] Upload Start: %s\n", filename.c_str());
                
                bool extValid = filename.endsWith(".bin");
                if (!extValid) {
                    DEBUG_PRINTLN("[OTA] Error: Invalid file extension!");
                    return; // 不开始 Update
                }
                
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                }
            }
            else if (upload.status == UPLOAD_FILE_WRITE) {
                if (Update.isRunning()) {
                    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                        Update.printError(Serial);
                    }
                }
            }
            else if (upload.status == UPLOAD_FILE_END) {
                if (Update.isRunning()) {
                    if (Update.end(true)) {
                        DEBUG_PRINTF("[OTA] Success: %u B\n", upload.totalSize);
                    } else {
                        Update.printError(Serial);
                    }
                }
            }
        }
    );
    
    // 6. 404 / Captive Portal
    server.onNotFound([this]() { handleNotFound(); });
}

// ==================== 具体实现 ====================

void WebConfig::handleRoot() {
    if (SPIFFS.exists("/index.html")) {
        File file = SPIFFS.open("/index.html", "r");
        server.streamFile(file, "text/html");
        file.close();
    } else {
        server.sendHeader("Location", "/cyber-void", true);
        server.send(302, "text/plain", "Redirect to CyberVoid");
    }
}

void WebConfig::handleCyberVoid() {
    String html = R"rawliteral(<!DOCTYPE html><html><head><meta charset='utf-8'><title>CyberVoid</title></head><body>
<h1>🌌 赛博虚空 🌌</h1>
<p>index.html 丢失... 请通过 OTA 上传固件或文件系统镜像修复。</p>
<form method='POST' action='/update' enctype='multipart/form-data'>
<input type='file' name='update'>
<input type='submit' value='OTA Update'>
</form></body></html>)rawliteral";
    server.send(200, "text/html", html);
}

void WebConfig::handleGetConfig() {
    String json = "{";
    json += "\"firmwareVersion\":\"" + String(NvsReadyChar_fixed) + "\",";
    json += "\"mode\":" + String((int)CRNT_MODE) + ",";
    json += "\"flushMinTime\":" + String(FLUSH_MIN_TIME_US) + ",";
    json += "\"flushThresTime\":" + String(FLUSH_THRES_TIME_US) + ",";
    json += "\"flushMinSize\":" + String(FLUSH_MIN_SIZE) + ",";
    json += "\"flushThresSize\":" + String(FLUSH_THRES_SIZE) + ",";
    json += "\"debugON\":" + String(DEBUG_ON ? 1 : 0) + ",";
    json += "\"serialDataNum\":" + String(NUM_S_DATA) + ",";
    json += "\"baudData\":" + String(BAUD_DATA) + ",";
    json += "\"baudDebug\":" + String(BAUD_DEBUG) + ",";
    json += "\"wifiSSID\":\"" + jsonEscape(HOME_WIFI_SSID) + "\",";
    json += "\"wifiPassword\":\"" + jsonEscape(HOME_WIFI_PWD) + "\",";
    json += "\"autoConnectWifi\":" + String(HOME_WIFI_AUTO ? 1 : 0) + ",";
    json += "\"sendPort\":" + String(PC_UDP_PORT) + ",";
    json += "\"listenPort\":" + String(ESP32_UDP_PORT) + ",";
    json += "\"sendBroad\":" + String(SEND_BROAD ? 1 : 0) + ",";
    json += "\"listenBroad\":" + String(LISTEN_BROAD ? 1 : 0) + ",";
    json += "\"wifiChannel\":" + String(WIFI_CHNL) + ",";
    json += "\"wifiChAuto\":" + String(WIFI_CH_AUTO ? 1 : 0) + ",";
    
    String staIP = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "0.0.0.0";
    json += "\"staIP\":\"" + staIP + "\"";
    
    json += "}";
    server.send(200, "application/json", json);
}

void WebConfig::handleSaveConfig() {
    if (!server.hasArg("mode")) {
        server.send(400, "text/plain", "Bad Request: missing mode");
        return;
    }
    
    // === 1. 验证 ===
    if (!validateFlushParams(server.arg("flushMinTime").toInt(), server.arg("flushThresTime").toInt(),
    server.arg("flushMinSize").toInt(), server.arg("flushThresSize").toInt())) {
        server.send(400, "text/plain", "Invalid Flush Params (min > thres or thresSize=0)");
        return;
    }
    
    if (!validateBaudRate(server.arg("baudData"))) {
        server.send(400, "text/plain", "Invalid BaudRate (1200~2000000)");
        return;
    }
    
    if (server.hasArg("wifiPassword") && !validateWiFiPassword(server.arg("wifiPassword"))) {
        server.send(400, "text/plain", "Invalid WiFi Password (0 or 8~63 ASCII chars)");
        return;
    }
    
    // === 2. 直接写入全局变量 (storeDataConfig 会从这些变量存到 NVS) ===
    CRNT_MODE = (Mode)server.arg("mode").toInt();
    FLUSH_MIN_TIME_US = server.arg("flushMinTime").toInt();
    FLUSH_THRES_TIME_US = server.arg("flushThresTime").toInt();
    FLUSH_MIN_SIZE = server.arg("flushMinSize").toInt();
    FLUSH_THRES_SIZE = server.arg("flushThresSize").toInt();
    
    NUM_S_DATA = server.arg("serialDataNum").toInt();
    BAUD_DATA = server.arg("baudData").toInt();
    if (server.hasArg("baudDebug")) {
        BAUD_DEBUG = server.arg("baudDebug").toInt();
    }
    if (server.hasArg("debugON")) {
        DEBUG_ON = (server.arg("debugON").toInt() == 1);
    }
    
    String newSSID = server.arg("wifiSSID");
    String newPWD = server.arg("wifiPassword");
    bool wifiChanged = (newSSID != HOME_WIFI_SSID) || (newPWD != HOME_WIFI_PWD);
    
    HOME_WIFI_SSID = newSSID;
    HOME_WIFI_PWD = newPWD;
    HOME_WIFI_AUTO = (server.arg("autoConnectWifi").toInt() == 1);
    
    PC_UDP_PORT = server.arg("sendPort").toInt();
    ESP32_UDP_PORT = server.arg("listenPort").toInt();
    SEND_BROAD = (server.arg("sendBroad").toInt() == 1);
    LISTEN_BROAD = (server.arg("listenBroad").toInt() == 1);
    WIFI_CHNL = server.arg("wifiChannel").toInt();
    WIFI_CH_AUTO = (server.arg("wifiChAuto").toInt() == 1);
    
    // === 3. 保存到 NVS ===
    bool saveSuccess = storeDataConfig();
    
    if (!saveSuccess) {
        server.send(500, "text/plain", "NVS Save Failed");
        return;
    }
    
    // === 4. 从 NVS 重新读取，确保全局变量与 NVS 完全同步 ===
    readDataConfig();
    
    server.send(200, "text/plain", "Saved");
    
    // === 5. 如果 WiFi 设置变了且开启自动连接，立即尝试重连 ===
    if (HOME_WIFI_AUTO && wifiChanged && HOME_WIFI_SSID.length() > 0) {
        DEBUG_PRINTLN("[WebConfig] WiFi settings changed, reconnecting...");
        WiFi.disconnect();
        WiFi.begin(HOME_WIFI_SSID.c_str(), HOME_WIFI_PWD.c_str());
    }
}

void WebConfig::handleNotFound() {
    String host = server.hostHeader();
    
    // 非本机地址全部重定向到 AP IP (强制门户)
    // esp-air.local 通过 mDNS 在 STA 模式下可用
    if (host != "10.0.0.1" && host != "esp-air.local") {
        DEBUG_PRINTF("[WebConfig] Captive Portal Redirect: %s -> 10.0.0.1\n", host.c_str());
        server.sendHeader("Location", "http://10.0.0.1/", true);
        server.send(302, "text/plain", "");
        return;
    }
    
    server.send(404, "text/plain", "Not Found");
}

String WebConfig::getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    return "text/plain";
}