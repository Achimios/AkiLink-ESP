#include "modules_run.h"

#include "air_tcp.h"
#include "air_udp.h"
#include "led_state.h"
#include "web_tune.h"
#ifndef FOR_ESP32_C3
#include "air_spp.h"
#endif
#include "air_ble.h"

// #include "air_espnow.h"

#ifdef WEB_TUNE_H
#define WEB_TUNE_BEGIN() webConfig.begin()
#define WEB_TUNE_UPDATE() webConfig.update()
#else
#define WEB_TUNE_BEGIN()
#define WEB_TUNE_UPDATE()
#endif

#ifdef AIR_TCP_H
#define AIR_TCP_BEGIN() airTcp_begin()
#define AIR_TCP_UPDATE() airTcp_update()
#else
#define AIR_TCP_BEGIN()
#define AIR_TCP_UPDATE()
#endif

#ifdef AIR_UDP_H
#define AIR_UDP_BEGIN() airUdp_begin()
#define AIR_UDP_UPDATE() airUdp_update()
#else
#define AIR_UDP_BEGIN()
#define AIR_UDP_UPDATE()
#endif

#ifdef AIR_SPP_H
#define AIR_SPP_BEGIN() airSpp_begin()
#define AIR_SPP_UPDATE() airSpp_update()
#else
#define AIR_SPP_BEGIN()
#define AIR_SPP_UPDATE()
#endif

#ifdef AIR_BLE_H
#define AIR_BLE_BEGIN() airBle_begin()
#define AIR_BLE_UPDATE() airBle_update()
#else
#define AIR_BLE_BEGIN()
#define AIR_BLE_UPDATE()
#endif

#ifdef AIR_ESPNOW_H
#define AIR_ESPNOW_BEGIN() airEspnow_begin()
#define AIR_ESPNOW_UPDATE() airEspnow_update()
#else
#define AIR_ESPNOW_BEGIN()
#define AIR_ESPNOW_UPDATE()
#endif

//   ▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//   █  █   █  █   █  █   █  █
//   █  █   █  █   █  █   █  █
//   ▀▀▀▀   ▀▀▀▀   ▀▀▀▀   ▀▀▀▀
void ButtonsInit() {

  blink_begin();  // 💡LED vTask任务💡

  pinMode(CFG_PIN, INPUT_PULLUP);
  lastCfgPin = !digitalRead(CFG_PIN);
  web_config_mode = lastCfgPin;
}
//   ▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//   █  █   █  █   █  █   █  █
//   █  █   █  █   █  █   █  █
//   ▀▀▀▀   ▀▀▀▀   ▀▀▀▀   ▀▀▀▀
void ButtonsUpdate() {
  // 检测CFG开关变化 → 300ms延迟防抖 → 重启
  static unsigned long cfgChangeTime = 0;
  static bool cfgChangePending = false;

  bool now = !digitalRead(CFG_PIN);
  if (now != lastCfgPin) {
    if (!cfgChangePending) {
      // 首次检测到变化 → 开始计时
      cfgChangePending = true;
      cfgChangeTime = millis();
    }
  } else {
    // 恢复原样 → 取消（抖动）
    cfgChangePending = false;
  }

  // 持续300ms确认不是抖动
  if (cfgChangePending && (millis() - cfgChangeTime >= 300)) {
    cfgChangePending = false;
    lastCfgPin = now;
    web_config_mode = now;

    if (LedTaskHandle != NULL) {   // 💡LED vTask任务💡
      vTaskDelete(LedTaskHandle);  // 任务直接原地消失
      LedTaskHandle = NULL;
    }

    digitalWrite(AirReadyIndicator_PIN, LOW);
    digitalWrite(StateIndicator_PIN, LOW);
    DEBUG_PRINTLN("CFG change (300ms debounce) → restart");
    delay(200);
    ESP.restart();
  }
}

//        ◤◢◣◥◤◢◣◥◤◢◣◥◤◢◣◥
//        ◣◥◤◢◣◥◤◢◣◥◤◢◣◥◤◢
//        ◢◣◥◤◢◣◥◤◢◣◥◤◢◣◥◤
//        ◥◤◢◣◥◤◢◣◥◤◢◣◥◤◢◣
void modulesInit() {
  // #endif  //绝对绝对不要在一个项目里同时调用 Arduino Stream.h 的 begin
  // 和底层驱动的 uart_driver_install，这会导致资源冲突和不可预期的行为。
#ifdef _FACTORY_DEBUG
  debugNvsCheck();
#endif
  checkNvsData();      // 检查 NVS 数据
  init_s_debug_raw();  //  ESP会自己初始化。所以没必要开。这里重配置只是为了让buf大点儿，打印不卡顿
  init_s_data_raw();   // 若都是uart0，就覆盖原参数

// 只要我在web tune把s data设置为uart0，矩阵就会再次绑定tx io17, rx io16到uart0上。tx io共存了，但是rx io被替换成16了。
// 现在在init_s_data_raw()里，我 if (NUM_S_DEBUG != NUM_S_DATA) uart_set_pin(NUM_S_DATA, 17, 16, -1, -1);就不会了
// _TIPS_里是其他时候遇到这问题该如何解决，和目前问题无关
#ifdef _TIPS_
                      //  ⚠️⚠️⚠️目前莫名会出现一个BUG,uart0 和 2的 tx共享对方数据，以下是解决办法
  // 复位 GPIO，断开所有现有映射
  gpio_reset_pin(GPIO_NUM_17);
  gpio_reset_pin(GPIO_NUM_16);

  // 设置为普通 GPIO（断开任何外设连接）
  gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_16, GPIO_MODE_INPUT);

  init_s_data_raw();  // 🔓🔓🔓🔓🔓

  // ⚠️⚠️⚠️解决uart0 和 2的 tx共享对方数据，以下是解决办法
  gpio_matrix_out(17, U2TXD_OUT_IDX, false, false);  // 只允许 UART2 TX 驱动 GPIO17
  gpio_matrix_in(16, U2RXD_IN_IDX, false);           // 只允许 GPIO16 输入到 UART2 RX

  // 确保 UART0 TX 只输出到 GPIO1，不要跑到 GPIO17
  gpio_matrix_out(1, U0TXD_OUT_IDX, false, false);
#endif

  if (DEBUG_ON) {
    delay(50);
    // 这里的 DEBUG_PRINTLN 会自动通过宏调用 printf 输出到底层驱动
    DEBUG_PRINTLN("ClapTrap's Coming 111");
    delay(50);
    DEBUG_PRINTLN("ClapTrap's Coming 222");
    delay(50);
    DEBUG_PRINTLN("ClapTrap's Coming 333");
    delay(50);
    DEBUG_PRINTLN("BOOT SUCCESS (Raw UART Mode)");
  }
  if (web_config_mode) {
    WEB_TUNE_BEGIN();
  } else {
    getMaxPayLoad();
    switch (CRNT_MODE) {
      case MODE_TCP: AIR_TCP_BEGIN(); break;
      case MODE_UDP: AIR_UDP_BEGIN(); break;
      case MODE_SPP: AIR_SPP_BEGIN(); break;
      case MODE_BLE: AIR_BLE_BEGIN(); break;
      case MODE_ESPNOW: AIR_ESPNOW_BEGIN(); break;
      default: break;
    }
  }
}

//        ◤◢◣◥◤◢◣◥◤◢◣◥◤◢◣◥
//        ◣◥◤◢◣◥◤◢◣◥◤◢◣◥◤◢
//        ◢◣◥◤◢◣◥◤◢◣◥◤◢◣◥◤
//        ◥◤◢◣◥◤◢◣◥◤◢◣◥◤◢◣
void modulesUpdate() {
  if (deviceConnected) {
  } else if (!deviceConnected) {
  }
  if (deviceConnected && !oldDeviceConnected)  // 连上的那一次
  {

    w2aLen = 0;              // w2a线性buf归零
    a2wTail = a2wHead;       //  a2w环形Buf归零（注意head不归零，保持原位，tail追上head就表示空了）
    a2wToWrite = 0;          // a2w本次要写入的字节数归零
    packetSize = 0;          // air包大小归零
    ringLack = 0;            // 环形buf状态归零
    lastFlushUs = micros();  // flush计时器归零

    DEBUG_PRINTLN(web_config_mode ? "成功连接，模式 = 网页调参" : ("成功连接，模式 = " + String(CRNT_MODE)));

    oldDeviceConnected = true;
  } else if (!deviceConnected && oldDeviceConnected)  // 断开的那一次
  {
    if (CRNT_MODE == MODE_BLE) bleReconnect();
    oldDeviceConnected = false;
  }

  if (web_config_mode) {
    WEB_TUNE_UPDATE();
  } else {
    switch (CRNT_MODE) {
      case MODE_TCP: AIR_TCP_UPDATE(); break;
      case MODE_UDP: AIR_UDP_UPDATE(); break;
      case MODE_SPP: AIR_SPP_UPDATE(); break;
      case MODE_BLE: AIR_BLE_UPDATE(); break;
      case MODE_ESPNOW: AIR_ESPNOW_UPDATE(); break;
      default: break;
    }
  }
  // blink_state(); //💡LED 函数💡
}
