#pragma once
#define WIFI_CONFIG_H
#include <WiFi.h>
#include "data_config.h"
// #include <ESPmDNS.h> // 引入 mDNS

#include <esp_wifi.h>

// #include <DNSServer.h>
// DNSServer dnsServer;
//固定值
#define AP_PWD "11111111"  //默认密码，必须8位以上
#define AP_NAME_WEB "WEB_网址10.0.0.1，密8个1"
#define AP_NAME_TCP "TCP_IP_10.0.0.1，密8个1"
#define AP_NAME_UDP "UDP_IP_10.0.0.1，密8个1"
#define AP_NAME_ESPNOW "EspNow_网页设置配对MAC"

#define AP_IP_COMMA 10, 0, 0, 1
#define AP_SUBNET_COMMA 255, 255, 255, 0  
#define AP_IP_BRDCST_COMMA 10, 0, 0, 255

#define AP_IP_STRING "10.0.0.1"  // 需要额外定义字符串版本

extern bool isAPMode;

bool connectToSTA() ;
void startAPMode() ;
void checkWifiConnection(); // 连接状态检查和重连逻辑
uint8_t scanAndSelectChannel(); 
void getLocalMacs();




