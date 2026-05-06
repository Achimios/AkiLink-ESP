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