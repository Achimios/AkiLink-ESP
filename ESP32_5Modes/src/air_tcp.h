// Manus 写的
#ifndef AIR_TCP_H
#define AIR_TCP_H

#include <Arduino.h>
#include "lwip/tcp.h" // 引入底层 lwIP TCP 库


// 移除 WiFiServer/WiFiClient 的 extern，改用底层 pcb 指针
extern struct tcp_pcb *client_pcb; 

void airTcp_begin();
void airTcp_update();

#endif
