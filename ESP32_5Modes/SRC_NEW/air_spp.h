// Manus写的
#pragma once
#define AIR_SPP_H

#include <Arduino.h>

// SPP 设备名称
#define ESP_SPP_NAME "ESP_串口转蓝牙SPP" 

// 函数声明
void airSpp_begin();
void airSpp_update();
