#include <Arduino.h>
#include "modules_run.h"

// ============================================================
//  ESP8266 / ESP8285  main.cpp
//  入口文件 — 极简壳，所有逻辑都在 modules_run 里
// ============================================================

void setup() {
    ButtonsInit();    // RTC 标志检查 + LED + GPIO0
    modulesInit();    // 串口 + LittleFS配置 + WiFi + TCP/UDP 或 WebConfig
}

void loop() {
    ButtonsUpdate();  // 长按检测 (0.5s → LED亮 → 松手 → RTC → 重启)
    modulesUpdate();  // 主业务轮询 + LED闪烁

    // ⚠️ ESP8266 是单核! WiFi协议栈和用户代码跑在同一个核上
    // yield() 让出CPU给WiFi/TCP/IP协议栈处理收发
    // 不调的话WiFi会卡死 (ESP32双核不需要这个)
    yield();
}