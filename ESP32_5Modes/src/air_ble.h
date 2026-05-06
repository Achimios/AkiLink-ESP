//air_ble.h
#pragma once
#define AIR_BLE_H
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
// #include "NimBLEDevice.h" // ESP32 BLE 库，支持 BLE 4.0 和 5.0

#include "data_config.h"
#include "buf_rules.h"


#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHAR_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHAR_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

extern BLECharacteristic* txChar;
extern BLEServer* pServer;

void airBle_begin();
void airBle_update();

#define ESP_BLE_NAME "ESP_SerialToBLE" // BLE设备名称，可以在手机蓝牙设置中看到

  // -------- 断线重广播--------
void bleReconnect();
