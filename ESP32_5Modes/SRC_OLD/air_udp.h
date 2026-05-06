#ifndef AIR_UDP_H
#define AIR_UDP_H
#include <Arduino.h>
#include <WiFiUdp.h>

#include "data_config.h"
#include "buf_rules.h"
#include "wifi_config.h"


// 全局变量声明
extern WiFiUDP udp;

// 函数声明
void airUdp_begin();
void airUdp_update();



#endif