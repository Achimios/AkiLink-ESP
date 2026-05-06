#include "modules_run.h"
#include "led_state.h"

#include "web_tune.h"
#include "air_tcp.h"
#include "air_udp.h"
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

void ButtonsInit()
{
  pinMode(AirReadyIndicator_PIN, OUTPUT);
  digitalWrite(AirReadyIndicator_PIN, LOW);
  pinMode(StateIndicator_PIN, OUTPUT);
  digitalWrite(StateIndicator_PIN, LOW);
  pinMode(CFG_PIN, INPUT_PULLDOWN);
  lastCfgPin = digitalRead(CFG_PIN);
  web_config_mode = lastCfgPin;
}
void ButtonsUpdate()
{
  // 检测重启
  bool now = digitalRead(CFG_PIN);
  if (now != lastCfgPin)
  {
    digitalWrite(AirReadyIndicator_PIN, LOW);
    digitalWrite(StateIndicator_PIN, LOW);
    DEBUG_PRINTLN("CFG change → restart");
    delay(200);
    ESP.restart();
  }
  lastCfgPin = now;
  web_config_mode = now;
}




void modulesInit()
{
#ifdef _FACTORY_DEBUG
  Serial.begin(FCT_BAUD_DBG);
  delay(50);
  Serial.println("Factory Debug Begins 111");
  delay(50);
  Serial.println("Factory Debug Begins 222");
  delay(50);
  Serial.println("Factory Debug Begins 333");
  debugNvsCheck();
#endif
  S_DATA->setRxBufferSize(uart_rx_soft_buf_size); // RX 软件缓冲区改成 1024
  // S_DATA->setTxBufferSize(uart_tx_soft_buf_size);  // ARDUINO 框架下没有暴露接口
  checkNvsData();
  S_DEBUG->begin(BAUD_DEBUG);
  S_DATA->begin(BAUD_DATA);
  if (DEBUG_ON)
  {
    delay(50); // 因为接电脑时会吞掉部分print。所以先多次重复
    DEBUG_PRINTLN("ClapTrap's Coming 111");
    delay(50);
    DEBUG_PRINTLN("ClapTrap's Coming 222");
    delay(50);
    DEBUG_PRINTLN("ClapTrap's Coming 333");
    delay(50);
    DEBUG_PRINTLN("BOOT");
  }
  if (web_config_mode)
  {
    WEB_TUNE_BEGIN();
  }
  else
  {
    getMaxPayLoad();
    switch (CRNT_MODE)
    {
    case MODE_TCP:
      AIR_TCP_BEGIN();
      break;
    case MODE_UDP:
      AIR_UDP_BEGIN();
      break;
    case MODE_SPP:
      AIR_SPP_BEGIN();
      break;
    case MODE_BLE:
      AIR_BLE_BEGIN();
      break;
    case MODE_ESPNOW:
      AIR_ESPNOW_BEGIN();
      break;
    default:
      break;
    }
  }
}

void modulesUpdate()
{
    if (deviceConnected)
  {
  }
  else if (!deviceConnected)
  {
  }
  if (deviceConnected && !oldDeviceConnected) //连上的那一次
  {
    uint8_t tempBuf[128];
    S_DATA->read(tempBuf, S_DATA->available()); // 连接时清空串口缓冲，防止旧数据干扰
    w2aLen = 0;  // w2a线性buf归零
    a2wTail = a2wHead; //  a2w环形Buf归零（注意head不归零，保持原位，tail追上head就表示空了）
    a2wToWrite = 0; // a2w本次要写入的字节数归零
    packetSize = 0;// air包大小归零
    ringLack = 0; // 环形buf状态归零
    lastFlushUs = micros();// flush计时器归零

    DEBUG_PRINTLN(web_config_mode
                      ? "成功连接，模式 = 网页调参"
                      : ("成功连接，模式 = " + String(CRNT_MODE)));

    oldDeviceConnected = true;
  }
  else if (!deviceConnected && oldDeviceConnected) //断开的那一次
  {
    oldDeviceConnected = false; 
  }


  if (web_config_mode)
  {
    WEB_TUNE_UPDATE();
  }
  else
  {
    switch (CRNT_MODE)
    {
    case MODE_TCP:
      AIR_TCP_UPDATE();
      break;
    case MODE_UDP:
      AIR_UDP_UPDATE();
      break;
    case MODE_SPP:
      AIR_SPP_UPDATE();
      break;
    case MODE_BLE:
      AIR_BLE_UPDATE();
      break;
    case MODE_ESPNOW:
      AIR_ESPNOW_UPDATE();
      break;
    default:
      break;
    }
  }

    blink_state();
}
