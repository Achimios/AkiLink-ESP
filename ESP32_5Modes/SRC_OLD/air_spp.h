#pragma once
#define AIR_SPP_H
#include <Arduino.h>
#include "BluetoothSerial.h"

#include "data_config.h"
#include "buf_rules.h"

extern BluetoothSerial SerialBT;

void airSpp_begin();
void airSpp_update();

#define ESP_SPP_NAME "ESP_SerialToSPP" // spp设备名称，可以在手机蓝牙设置中看到