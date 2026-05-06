#pragma once
#define AIR_TCP_H
#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "data_cfg.h"
#include "buf_rules.h"
#include "wifi_config.h"

// ============================================================
//  ESP8266 / ESP8285  air_tcp.h
//  Arduino WiFiServer/WiFiClient — 和ESP32 API一致
// ============================================================

// TCP 不使用 a2wRing 环形缓冲区!
// lwIP TCP 自带 RX 窗口 (~5744B)，相当于内置ring buf
// Air→Wire: tcpClient.read() → 小线性buf → Serial.write()
// Wire→Air: serialToW2aBuf() → shouldFlush() → tcpClient.write()

extern WiFiServer tcpServer;
extern WiFiClient tcpClient;

void airTcp_begin();
void airTcp_update();
