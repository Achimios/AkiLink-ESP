#pragma once
#define AIR_UDP_H
#include <Arduino.h>
#include <WiFiUdp.h>

#include "data_cfg.h"
#include "buf_rules.h"
#include "wifi_config.h"

// ============================================================
//  ESP8266 / ESP8285  air_udp.h
//  WiFiUDP — Arduino API，和ESP32完全一样
// ============================================================

extern WiFiUDP udp;

void airUdp_begin();
void airUdp_update();
