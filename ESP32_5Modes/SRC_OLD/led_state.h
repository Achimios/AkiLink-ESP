#pragma once
#define LED_STATE_H
#include <Arduino.h>
#include "data_config.h"

extern unsigned long sequenceLastTime;
extern unsigned long liteLastTime;
extern bool sequenceON;
extern bool liteON;

void blink_begin();
void blink_method(uint16_t liteONtime, uint16_t liteOFFtime, uint8_t blinkTimes, uint16_t sequenceInterval);
void blink_state();