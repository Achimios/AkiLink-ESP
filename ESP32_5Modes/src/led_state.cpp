#include "led_state.h"

// 定义 LED 任务句柄
TaskHandle_t LedTaskHandle = NULL;
// bool firstBoot = true;
// unsigned long bootTime;

// --- 真正的独立 LED 任务 ---
void ledTask(void* pvParameters) {
  (void)pvParameters;

  // 初始化引脚（放到这里或者 setup 都可以）
  pinMode(AirReadyIndicator_PIN, OUTPUT);
  digitalWrite(AirReadyIndicator_PIN, LOW);
  pinMode(StateIndicator_PIN, OUTPUT);
  digitalWrite(StateIndicator_PIN, LOW);

  for (;;) {  // 死循环，任务永不结束

    // --- 1. 处理连接状态 (优先级最高) ---
    if (deviceConnected) {
      digitalWrite(StateIndicator_PIN, HIGH);

      // 处理 AirReady 逻辑
      if (CRNT_MODE != MODE_TCP) {
        digitalWrite(AirReadyIndicator_PIN, HIGH);
      } else {
        // TCP 模式下看是否有客户端
        digitalWrite(AirReadyIndicator_PIN, tcpClientConnected ? HIGH : LOW);
      }

      // 既然连接了，就没必要闪烁了，每 100ms 检查一次状态即可
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;  // 直接跳过后面的闪烁逻辑，进入下一次循环
    }

    // --- 2. 未连接：进入闪烁模式 ---
    digitalWrite(AirReadyIndicator_PIN, LOW);  // 没连接 AirReady 肯定灭

    // 定义闪烁参数
    int onTime = 180;
    int offTime = 120;
    int count = 1;
    int seqInterval = 1000;

    // clang-format off
    // 根据模式配置参数
    if (web_config_mode) {
      onTime=400; offTime=400; count=5; seqInterval=400;
    } else {
      switch (CRNT_MODE) {
        case MODE_TCP:    count = 1; seqInterval = 2200; break; // 这里假设 MODE_TCP 是 1
        case MODE_UDP:    count = 2; seqInterval = 1900; break;
        case MODE_SPP:    count = 3; seqInterval = 1600; break;
        case MODE_BLE:    count = 4; seqInterval = 1300; break;
        case MODE_ESPNOW: count = 5; seqInterval = 1000; break;
        default:          count = 1; seqInterval = 1000; break;
      }
    }
    // clang-format on

    // --- 3. 执行闪烁序列 (线性逻辑，简单粗暴) ---
    for (int i = 0; i < count; i++) {
      // 亮
      digitalWrite(StateIndicator_PIN, HIGH);
      vTaskDelay(pdMS_TO_TICKS(onTime));

      // 灭
      digitalWrite(StateIndicator_PIN, LOW);

      // 如果不是最后一次闪烁，需要等待 offTime
      // 如果是最后一次，直接去等 seqInterval 也可以，看你喜好
      if (i < count - 1) { vTaskDelay(pdMS_TO_TICKS(offTime)); }
    }

    // --- 4. 序列间隔 ---
    // 这一步直接阻塞，让出 CPU 给 UDP 任务去跑洪水！
    // 在这 seqInterval 期间，这个 LED 任务完全不占用任何 CPU 资源！
    vTaskDelay(pdMS_TO_TICKS(seqInterval));

#ifdef _FACTORY_DEBUG_VTASK  // 别TM在栈里 查询并打印自己的栈剩余了，容易爆栈
    // 建议每隔一段时间打印一次，或者在特定条件下触发
    uint32_t threadValue = ulTaskNotifyTake(
        pdTRUE, pdMS_TO_TICKS(10));  // 如果设置为永久等待portMAX_DELAY，2KB也直接爆栈。因为要存储所有临时信息
    if (threadValue > 0) {
      UBaseType_t remainingStack = uxTaskGetStackHighWaterMark(NULL);  // NULL 代表查自己。别查自己了，爆栈啊！
      _FACTORY_DEBUG_PRINT("LED任务剩余栈空间 (Byte): ");              // 别TM把打印丢 任务栈里，太占
      _FACTORY_DEBUG_PRINTLN(remainingStack);
    }  // else{}
#endif
#ifdef _FACTORY_DEBUG_VTASK  // xTaskNotifyGive 触发通知，这部分丢主循环
    static unsigned long callLedTime = millis();
    static bool callLed = true;
    if (!callLed && millis() - callLedTime > 5000) {
      if (LedTaskHandle != NULL) {
        callLed = false;
        // 踢一下 LED 任务的屁股
        xTaskNotifyGive(LedTaskHandle);  // xTaskNotifyGive 会把 通知数+1
      }
    }
#endif
  }  // for
}  // vTask



// 在 setup() 里启动它
void blink_begin() {
  // ... 其他初始化 ...
  xTaskCreatePinnedToCore(ledTask,         // 1. 任务函数指针
                          "LED_Task",      // 2. 任务名称 (字符串)
                          2048,            // 3. 栈深度 (字节) //动态变化，还是给大点儿吧稳点儿，320KB堆不怕
                          NULL,            // 4. 传给任务的参数
                          1,               // 5. 优先级 (0-24，越大越高) (如果闪烁卡顿改成2或3)
                          &LedTaskHandle,  // 6. 任务句柄 (用来遥控它)
                          0                // 7. 绑定核心 ID (0 或 1) （优先级低，应该不阻塞wifi）
  );
}