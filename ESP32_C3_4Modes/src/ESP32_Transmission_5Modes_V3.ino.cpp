#include <Arduino.h>
// #include "data_config.h"
#include "led_state.h"

#include "modules_run.h"

void setup() {  // ⛄️⛄️⛄️⛄️⛄️⛄️⛄️⛄️
  ButtonsInit();
  modulesInit();
}

void loop() {  // 🦍🦍🦍🦍🦍🦍🦍
  ButtonsUpdate();
  modulesUpdate();

  // delayMicroseconds(100);  // 它不会被优化成 vTaskDelay。因为 vTaskDelay 的最小单位是系统节拍（Tick），在 ESP32
  // 上默认是 1 毫秒（1000微秒）。它处理不了你想要的几十或几百微秒。

  yield();  // ESP32 的 WiFi 协议栈主要跑在 Core 0。如果你在 Core 1 猛跑 loop 而不给 Core 0
  // 喘息（通过共享资源或中断），Core 0 可能会卡住。但在 Core 1 用 yield() 对 Core 0
  // 的帮助并不直接，除非你调用的是 vTaskDelay(1)，这会强制进入阻塞态，释放更多系统资源。

  // vTaskDelay(pdMS_TO_TICKS(1));
}

// leave some spaces....


/**
 * ═══════════════════════════════════════════════════════════════
 *  air_udp.cpp — Air<->Wire UDP 透传核心逻辑（ESP-IDF 版）
 * ═══════════════════════════════════════════════════════════════
 *
 *  本文件实现了 Air<->Wire 双向数据流的核心逻辑：
 *   - Air -> Wire: 从环形缓冲区读取数据，通过 UART 发送到飞控
 *   - Wire -> Air: 从 UART 接收数据，封装成 UDP 包发送到 PC
 *
 *  设计原则：
 *    - 严格分离 Air/Wire 逻辑，保持代码清晰
 *    - 使用 lwIP 的 UDP API 实现高效网络通信
 *    - 集中处理 Wi-Fi 状态，确保网络稳定性
 *    - 不依赖任何 Arduino 头文件，纯 ESP-IDF 实现
 *
 *  注意事项：
 *    - 本文件不包含任何硬件引脚定义或全局配置，
 *      所有参数均从 app_config.h 引入。
 *    - Wi-Fi 连接状态由 wifi_setup.h 提供的接口查询，
 *      确保在主循环中定期调用 checkWifiConnection()。
 *    - UDP 包的目标地址和端口由全局变量控制，
 *      支持动态切换广播/单播模式。
 *
 * ═══════════════════════════════════════════════════════════════
 */