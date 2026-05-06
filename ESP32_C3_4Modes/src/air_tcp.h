#ifndef AIR_TCP_H
#define AIR_TCP_H
#include <Arduino.h>

#include "data_config.h"
#include "buf_rules.h"
#include "wifi_config.h"

#define _NO_NAGLE

extern WiFiServer tcpServer;
extern WiFiClient tcpClient;

void airTcp_begin();
void airTcp_update();




#endif