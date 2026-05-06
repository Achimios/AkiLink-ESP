// ============================================================
//  ESP8266 / ESP8285  web_tune.cpp
//  Ported from ESP32_Transmission_5Modes_V3/web_tune.cpp
//  SPIFFS → LittleFS, WebServer → ESP8266WebServer
//  OTA 暂时禁用 (ESP-01S 1MB Flash 不够双分区)
// ============================================================

#include "web_tune.h"

WebConfig webConfig;


// ==================== 辅助验证函数 ====================

bool WebConfig::validateWiFiPassword(const String& pwd) {
    if (pwd.length() == 0) return true;  // 允许开放网络
    if (pwd.length() < 8 || pwd.length() > 63) return false;
    for (size_t i = 0; i < pwd.length(); i++) {
        if (pwd.charAt(i) < 32 || pwd.charAt(i) > 126) return false;
    }
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
        if (c == '"')       out += "\\\"";
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
    DEBUG_PRINTLN("[Web] Starting...");

    // 1. LittleFS 应该在 checkStoredConfig() 中已经 mount 过
    //    这里做安全检查
    if (!LittleFS.begin()) {
        DEBUG_PRINTLN("[Web] LittleFS mount failed!");
        // 不 return，仍可提供 CyberVoid 备用页面
    }

    // 2. 获取MAC地址
    getLocalMacs();

    // 3. 启动网络 (AP+STA 或 纯AP)
    setupNetwork();

    // 4. 设置路由
    setupRoutes();

    // 5. 启动 Web 服务器
    server.begin();

    // 6. 启动 DNS (强制门户) — 所有域名解析到 AP IP
    dnsServer.start(53, "*", WiFi.softAPIP());

    active = true;
    DEBUG_PRINTLN("[Web] HTTP Server on port 80");
}

void WebConfig::update() {
    if (!active) return;

    // 处理 DNS + Web 请求
    dnsServer.processNextRequest();
    server.handleClient();

    // 监控连接状态变化
    static bool lastConnected = false;
    bool staConnected = (WiFi.status() == WL_CONNECTED);
    bool apHasClient  = (WiFi.softAPgetStationNum() > 0);
    bool currConnected = (staConnected || apHasClient);

    if (currConnected != lastConnected) {
        lastConnected = currConnected;
        deviceConnected = currConnected;

        if (currConnected) {
            DEBUG_PRINTLN("[WiFi] Connected!");
            if (staConnected) {
                DEBUG_PRINTF("[WiFi] STA IP: %s\n", WiFi.localIP().toString().c_str());
            }
        } else {
            DEBUG_PRINTLN("[WiFi] Disconnected");
        }
    }
}

void WebConfig::setupNetwork() {
    // AP+STA 模式: AP 供手机配置，同时尝试连家庭WiFi
    WiFi.mode(WIFI_AP_STA);

    // 配置 AP
    IPAddress apIP(AP_IP_COMMA);
    IPAddress gateway(AP_IP_COMMA);
    IPAddress subnet(AP_SUBNET_COMMA);

    WiFi.softAPConfig(apIP, gateway, subnet);
    WiFi.softAP(AP_NAME_WEB, AP_PWD);

    _FACTORY_PRINTF("[Web] AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    // 尝试连接 STA (非阻塞)
    if (HOME_WIFI_AUTO && HOME_WIFI_SSID.length() > 0) {
        _FACTORY_PRINTF("[Web] Auto-connecting STA: %s\n", HOME_WIFI_SSID.c_str());
        WiFi.begin(HOME_WIFI_SSID.c_str(), HOME_WIFI_PWD.c_str());
    }

    // 关闭省电
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
}


// ==================== 路由处理 ====================

void WebConfig::setupRoutes() {
    // 主页 — 从 LittleFS 读取 /index.html
    server.on("/", HTTP_GET, [this]() { handleRoot(); });

    // 赛博虚空 — index.html 丢失时的备用页
    server.on("/cyber-void", HTTP_GET, [this]() { handleCyberVoid(); });

    // 获取当前配置 (JSON)
    server.on("/get-config", HTTP_GET, [this]() { handleGetConfig(); });

    // 保存配置 (POST)
    server.on("/save-config", HTTP_POST, [this]() { handleSaveConfig(); });

    // OTA: ESP-01S 1MB Flash 不够双分区OTA，暂时禁用
    // 如果以后用 ESP-12F/ESP-12S (4MB)，可以打开

    // 404 / 强制门户重定向
    server.onNotFound([this]() { handleNotFound(); });
}


// ==================== 具体实现 ====================

void WebConfig::handleRoot() {
    if (LittleFS.exists("/index.html")) {
        File file = LittleFS.open("/index.html", "r");
        server.streamFile(file, "text/html");
        file.close();
    } else {
        // index.html 不存在 → 重定向到备用页
        server.sendHeader("Location", "/cyber-void", true);
        server.send(302, "text/plain", "Redirect to CyberVoid");
    }
}

void WebConfig::handleCyberVoid() {
    String html = R"rawliteral(<!DOCTYPE html><html><head><meta charset='utf-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>CyberVoid</title></head><body style='font-family:sans-serif;padding:20px;'>
<h1>🌌 赛博虚空 🌌</h1>
<p>index.html 丢失!</p>
<p>请使用 PlatformIO 重新上传文件系统:<br>
<code>pio run -t uploadfs</code></p>
<p>确保 data/index.html 文件存在。</p>
</body></html>)rawliteral";
    server.send(200, "text/html", html);
}

void WebConfig::handleGetConfig() {
    // 手动拼JSON — 比ArduinoJson省RAM (这个JSON只有几百字节)
    String json = "{";
    json += "\"firmwareVersion\":\"" + CfgReadyTag_fixed + "\",";
    json += "\"mode\":" + String((int)CRNT_MODE) + ",";
    json += "\"flushMinTime\":" + String(FLUSH_MIN_TIME_US) + ",";
    json += "\"flushThresTime\":" + String(FLUSH_THRES_TIME_US) + ",";
    json += "\"flushMinSize\":" + String(FLUSH_MIN_SIZE) + ",";
    json += "\"flushThresSize\":" + String(FLUSH_THRES_SIZE) + ",";
    json += "\"debugON\":" + String(DEBUG_ON ? 1 : 0) + ",";
    json += "\"baudData\":" + String(BAUD_DATA) + ",";
    json += "\"baudDebug\":" + String(BAUD_DEBUG) + ",";
    json += "\"wifiSSID\":\"" + jsonEscape(HOME_WIFI_SSID) + "\",";
    json += "\"wifiPassword\":\"" + jsonEscape(HOME_WIFI_PWD) + "\",";
    json += "\"autoConnectWifi\":" + String(HOME_WIFI_AUTO ? 1 : 0) + ",";
    json += "\"sendPort\":" + String(PC_UDP_PORT) + ",";
    json += "\"listenPort\":" + String(ESP_UDP_PORT) + ",";
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
    if (!validateFlushParams(
            server.arg("flushMinTime").toInt(), server.arg("flushThresTime").toInt(),
            server.arg("flushMinSize").toInt(), server.arg("flushThresSize").toInt())) {
        server.send(400, "text/plain", "Invalid Flush Params");
        return;
    }
    if (!validateBaudRate(server.arg("baudData"))) {
        server.send(400, "text/plain", "Invalid BaudRate (1200~2000000)");
        return;
    }
    if (server.hasArg("wifiPassword") && !validateWiFiPassword(server.arg("wifiPassword"))) {
        server.send(400, "text/plain", "Invalid WiFi Password (0 or 8~63 ASCII)");
        return;
    }

    // === 2. 写入全局变量 ===
    CRNT_MODE = (Mode)server.arg("mode").toInt();
    FLUSH_MIN_TIME_US   = server.arg("flushMinTime").toInt();
    FLUSH_THRES_TIME_US = server.arg("flushThresTime").toInt();
    FLUSH_MIN_SIZE      = server.arg("flushMinSize").toInt();
    FLUSH_THRES_SIZE    = server.arg("flushThresSize").toInt();

    // ESP8266 串口固定，不存serialDataNum
    BAUD_DATA = server.arg("baudData").toInt();
    if (server.hasArg("baudDebug")) {
        BAUD_DEBUG = server.arg("baudDebug").toInt();
    }
    if (server.hasArg("debugON")) {
        DEBUG_ON = (server.arg("debugON").toInt() == 1);
    }

    String newSSID = server.arg("wifiSSID");
    String newPWD  = server.arg("wifiPassword");
    bool wifiChanged = (newSSID != HOME_WIFI_SSID) || (newPWD != HOME_WIFI_PWD);

    HOME_WIFI_SSID = newSSID;
    HOME_WIFI_PWD  = newPWD;
    HOME_WIFI_AUTO = (server.arg("autoConnectWifi").toInt() == 1);

    PC_UDP_PORT  = server.arg("sendPort").toInt();
    ESP_UDP_PORT = server.arg("listenPort").toInt();
    SEND_BROAD   = (server.arg("sendBroad").toInt() == 1);
    LISTEN_BROAD = (server.arg("listenBroad").toInt() == 1);
    WIFI_CHNL    = server.arg("wifiChannel").toInt();
    WIFI_CH_AUTO = (server.arg("wifiChAuto").toInt() == 1);

    // === 3. 保存到 LittleFS ===
    bool ok = storeDataConfig();
    if (!ok) {
        server.send(500, "text/plain", "Save Failed");
        return;
    }

    // === 4. 重新读取，确保同步 ===
    readDataConfig();

    server.send(200, "text/plain", "Saved");

    // === 5. WiFi变了且自动连接 → 立即重连 ===
    if (HOME_WIFI_AUTO && wifiChanged && HOME_WIFI_SSID.length() > 0) {
        DEBUG_PRINTLN("[Web] WiFi changed, reconnecting...");
        WiFi.disconnect();
        WiFi.begin(HOME_WIFI_SSID.c_str(), HOME_WIFI_PWD.c_str());
    }
}

void WebConfig::handleNotFound() {
    String host = server.hostHeader();

    // 强制门户: 非本机域名全部重定向到 AP IP
    if (host != AP_IP_STRING) {
        server.sendHeader("Location", "http://" AP_IP_STRING "/", true);
        server.send(302, "text/plain", "");
        return;
    }

    server.send(404, "text/plain", "Not Found");
}


// Leave Some Spaces.....
