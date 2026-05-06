#pragma once
#define WEB_TUNE_H

#include <Arduino.h>
#include <ESP8266WebServer.h>  // ESP8266 内置同步 Web 服务器
#include <DNSServer.h>         // 强制门户 DNS
#include <LittleFS.h>

#include "wifi_config.h"
#include "data_cfg.h"

// ============================================================
//  ESP8266 / ESP8285  web_tune.h
//  Web 配置界面 — HTML 由 LittleFS 提供 (/index.html)
//  OTA: ESP8266 支持 Updater 库，但 ESP-01S 1MB Flash
//       固件+FS占满后没有OTA空间，暂时禁用
// ============================================================

class WebConfig {
public:
    void begin();    // 启动文件系统、网络、Web服务
    void update();   // 必须在 loop() 中调用
    bool isActive() { return active; }

private:
    ESP8266WebServer server{80};
    DNSServer dnsServer;
    bool active = false;

    // 核心设置
    void setupNetwork();
    void setupRoutes();

    // 请求处理
    void handleRoot();
    void handleGetConfig();
    void handleSaveConfig();
    void handleNotFound();
    void handleCyberVoid();

    // 辅助
    String jsonEscape(const String& s);

    // 验证
    bool validateWiFiPassword(const String& pwd);
    bool validateBaudRate(const String& baud);
    bool validateFlushParams(int minTime, int thresTime, int minSize, int thresSize);
};

extern WebConfig webConfig;
