#pragma once
#define WEB_TUNE_H

#include <Arduino.h>
#include <WebServer.h>     // 官方同步 Web 服务器
#include <DNSServer.h>     // 强制门户 DNS
// #include <LittleFS.h>
#include <SPIFFS.h>
#include <Update.h>        // OTA

#include "wifi_config.h"   // 你的宏定义
#include "data_config.h"   // 全局数据

class WebConfig {
public:
  void begin();    // 启动 Web 服务和网络
  void update();   // 必须在 loop() 中调用
  bool isActive() { return active; }

private:
  WebServer server{80};
  DNSServer dnsServer;
  bool active = false;

  // === 核心设置 ===
  void setupNetwork();
  void setupRoutes();
  
  // === 请求处理 (Handlers) ===
  void handleRoot();
  void handleGetConfig();
  void handleSaveConfig();
  void handleNotFound();
  void handleCyberVoid();
  
  // === 辅助 ===
  String getContentType(String filename);
  String jsonEscape(const String& s);  // JSON 字符串转义

  // === 验证函数 ===
  bool validateWiFiPassword(const String& pwd);
  bool validateBaudRate(const String& baud);
  bool validateFlushParams(int minTime, int thresTime, int minSize, int thresSize);
};

extern WebConfig webConfig;