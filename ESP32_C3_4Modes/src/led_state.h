#pragma once
#define LED_STATE_H
#include <Arduino.h>

#include "data_config.h"

extern TaskHandle_t LedTaskHandle;
void blink_begin();

#ifdef _WHAT_EVER_VTASK
// 1. 定义任务句柄（可选）
TaskHandle_t HeartbeatTaskHandle = NULL;

// 2. 任务函数：必须是这个格式
void heartbeatTask(void* pvParameters) {
  (void)pvParameters;  // 防止编译器报错说参数没用

  // 可以在这里加个串口提示
  Serial.println(">>> 异步心跳任务启动！同步率 400%！");

  for (;;) {  // FreeRTOS 任务必须是个死循环
    // --- 这里写你要独立运行的代码 ---
    digitalWrite(2, !digitalRead(2));  // 假设 LED 在 GPIO 2

    // 关键：绝对不能用 delay()！要用 FreeRTOS 的阻塞延时
    // pdMS_TO_TICKS 会把毫秒转换成系统节拍数
    vTaskDelay(pdMS_TO_TICKS(500));

    // 顺便打印点什么，证明它活着
    // Serial.print("[RTOS] 心跳正常... ");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);

  // 3. 创建任务！
  xTaskCreatePinnedToCore(heartbeatTask,         // 任务函数名
                          "Heartbeat",           // 任务名称（调试用）
                          2048,                  // 栈大小（字节）
                          NULL,                  // 传给任务的参数
                          1,                     // 优先级 (0-24，越大越高)
                          &HeartbeatTaskHandle,  // 句柄
                          0                      // 绑定到核心 0 (WiFi 核心)
  );

  // 注意：loop() 默认跑在 Core 1。我们把闪灯放 Core 0，
  // 这样哪怕 Core 1 被你的 80Mbps 洪水冲垮了，灯照样闪！
}

void loop() {
  // 这里的 loop 依然该干嘛干嘛，不用写任何定时器逻辑了！

  // 比如在某个地方黑进了系统，想强行停止 LED 闪烁
  if (HeartbeatTaskHandle != NULL) {
    动态改优先级：vTaskPrioritySet(LedTaskHandle, 10);
    查询剩余栈：uxTaskGetStackHighWaterMark(LedTaskHandle);

    暂停任务：vTaskSuspend(LedTaskHandle);
    恢复任务：vTaskResume(LedTaskHandle);
    删除任务：vTaskDelete(HeartbeatTaskHandle);
    HeartbeatTaskHandle = NULL;
  }
}



ulTaskNotifyTake();  任务里等指令：
xTaskNotifyGive(LedTaskHandle);   主循环发指令：
void ledTask(void* pvParameters) {
  for (;;) {
    // 任务在这里“入定”，完全不消耗 CPU，直到有人给它发通知
    // 睡觉期间主循环可能通知多次， pdTRUE：函数返回时将计数器清零。pdFALSE，不清零，但是 ulTaskNotifyTake 会导致-1次
    // 第二个参数 portMAX_DELAY：死等，不等到通知不起来
    uint32_t threadValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);// ulTaskNotifyTake 会在函数返回时后把 通知数-1

    if (threadValue > 0) {//threadValue目前是1
      // 收到指令了！执行一次特殊的闪烁
      digitalWrite(StateIndicator_PIN, HIGH);
      vTaskDelay(pdMS_TO_TICKS(100));
      digitalWrite(StateIndicator_PIN, LOW);
      Serial.println(">>> [RTOS] 已收到主循环指令，响应闪烁！");
    }
  }
}
// --- 主循环 (Core 1) ---
void loop() {
  if (收到某个特殊UDP包) {
    if (LedTaskHandle != NULL) {
      // 踢一下 LED 任务的屁股
      xTaskNotifyGive(LedTaskHandle); //xTaskNotifyGive 会把 通知数+1
    }
  }
}



#endif