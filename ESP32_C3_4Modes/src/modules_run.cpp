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
  if (CFG_PIN == 3) pinMode(4, INPUT_PULLDOWN);  // 4在3旁边，所以按钮可以接在3,4上，短接4就是下拉
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
    digitalWrite(StateIndicator_PIN, HIGH);  // 高为熄灯
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

  checkNvsData();  // 检查 NVS 数据
  Serial.begin(BAUD_DEBUG);
  switch (NUM_S_DATA) {
    case 0: Serial.begin(BAUD_DATA);  // HWCDC::begin()break;
    case 1: Serial1.begin(BAUD_DATA, SERIAL_8N1, DAT_RX_PIN, DAT_TX_PIN); break;
  }



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
