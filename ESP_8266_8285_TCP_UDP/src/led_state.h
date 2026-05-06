#pragma once
#define LED_STATE_H
#include <Arduino.h>
#include "data_cfg.h"

// ============================================================
//  ESP8266 / ESP-01S LED 状态指示
// ============================================================
//
//  ⚠️ GPIO2 复用冲突:
//    _FACTORY_DEBUG 编译期启用  → Serial1 占用 GPIO2 → LED 不可用
//    DEBUG_ON 运行时启用        → Serial1 占用 GPIO2 → LED 不可用
//    两者都关闭时               → GPIO2 作为状态LED使用
//
//  ESP-01S 只有一个LED (GPIO2, 低电平点亮)
//  通过闪烁模式区分状态:
//    Web配置模式  → 快闪 5次/组
//    TCP等待连接  → 闪1次/组
//    UDP等待连接  → 闪2次/组
//    已连接       → 常亮
// ============================================================

extern unsigned long sequenceLastTime;
extern unsigned long liteLastTime;
extern bool sequenceON;
extern bool liteON;
extern bool ledAvailable;   // LED是否可用 (GPIO2未被Serial1占用)

void led_begin();           // 初始化LED，检测是否可用
void blink_method(uint16_t liteONtime, uint16_t liteOFFtime,
                  uint8_t blinkTimes, uint16_t sequenceInterval);
void blink_state();         // 在 loop() 中调用
