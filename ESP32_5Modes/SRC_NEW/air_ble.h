// Gemini Pro 写的
#pragma once
#define AIR_BLE_H

#include <Arduino.h>
// 👇 替换原有的 BLE 头文件
#include <NimBLEDevice.h> 

#include "data_config.h"
#include "buf_rules.h"

// UUID 保持不变
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHAR_UUID_RX           "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHAR_UUID_TX           "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// 声明外部变量
extern NimBLECharacteristic* txChar;
extern NimBLEServer* pServer;

void airBle_begin();
void airBle_update();

#define ESP_BLE_NAME "ESP_串口转BLE"