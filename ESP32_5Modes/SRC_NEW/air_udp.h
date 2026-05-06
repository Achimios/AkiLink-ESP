#ifndef AIR_UDP_H
#define AIR_UDP_H

#include <Arduino.h>
#include "lwip/udp.h" // 引入底层 lwIP UDP 库


// 移除 WiFiUDP 实例，改用底层 pcb 指针
extern struct udp_pcb *air_udp_pcb;

void airUdp_begin();
void airUdp_update();

#endif
