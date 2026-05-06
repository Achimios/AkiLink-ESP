#pragma once
#define WIFI_CONFIG_H

#include <ESP8266WiFi.h>
#include "data_cfg.h"

// ============================================================
//  ESP8266 / ESP8285  wifi_config.h
//  AP/STA模式管理 — 仅TCP & UDP
// ============================================================

// AP网络参数 — 固定值
#define AP_IP_COMMA       10, 0, 0, 1
#define AP_SUBNET_COMMA   255, 255, 255, 0
#define AP_IP_BRDCST_COMMA 10, 0, 0, 255
#define AP_IP_STRING      "10.0.0.1"

// 当前是否为AP模式
extern bool isAPMode;

// WiFi 管理函数
bool connectToSTA();            // 尝试连接家庭WiFi (STA模式)
void startAPMode();             // 启动热点 (AP模式)
void checkWifiConnection();     // 连接状态检查
uint8_t scanAndSelectChannel(); // 扫描并选择最优信道
void getLocalMacs();            // 获取自身STA/AP MAC地址
