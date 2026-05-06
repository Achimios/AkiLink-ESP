#pragma once
#define WIFI_CONFIG_H
#include "data_config.h"
#include <WiFi.h>
#include <esp_wifi.h>  //调用底层api，修改射频功率等用，esp_wifi_set_config啥的

#include <DNSServer.h>

extern DNSServer dnsServer;

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
uint8_t scanAndSelectChannel(); 
void getLocalMacs();
void checkWifiConnection(); // 连接状态检查和重连逻辑