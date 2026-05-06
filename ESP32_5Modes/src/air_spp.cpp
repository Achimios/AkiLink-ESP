#include "air_spp.h"

#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"

// #include "stack/btm_api.h"    //stack/btm_api.h 是蓝牙协议栈（Bluedroid）内部的，通常不在搜索路径里。

// --- 明日香的“暴力通透”黑魔法 ---
extern "C" {
    // 强制声明这个函数，链接器会自动去库里找它的实现
    esp_err_t esp_bt_gap_set_link_policy(esp_bd_addr_t bda, uint16_t policy);
}

// 对应的策略宏定义（如果找不到就直接手动写死）
#ifndef ESP_BT_LINK_POLICY_STR_NONE
#define ESP_BT_LINK_POLICY_STR_NONE 0x0000
#endif
// 蓝牙事件回调函数
void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    if (event == ESP_SPP_SRV_OPEN_EVT) {
        // 当连接成功开启时
        Serial.println(">>> [SPP] 连接已开启，正在锁定 Active Mode...");
        
        // 关键指令：获取当前连接的远程设备地址 (bda)
        // 并强制设置 Link Policy 为“禁止所有节能模式”
        // ESP_BT_LINK_POLICY_STR_NONE 表示禁用 Sniff/Hold/Park
        esp_bt_gap_set_link_policy(param->srv_open.rem_bda, ESP_BT_LINK_POLICY_STR_NONE);
        
        Serial.println(">>> [GAP] 已禁用 Sniff Mode，链路现在处于全速状态！");
    }
}

BluetoothSerial SerialBT;

void airSpp_begin() {
  btStop();                      // 关闭 BLE
  SerialBT.begin(ESP_SPP_NAME);  // SPP

  DEBUG_PRINTLN("ESP32 SPP Bridge (Buffered)");
  //   DEBUG_PRINTLN(ESP_GATTS_MTU_EVT);
  //   DEBUG_PRINTLN(ESP_SPP_CONG_EVT);
  //   获得 SPP 缓冲区大小
}

void airSpp_update() {
  // ---------- Client check ----------
  if (!SerialBT.hasClient()) {
    deviceConnected = false;
    return;
  } else {
    deviceConnected = true;
  }


  int16_t avai = SerialBT.available();
  // static unsigned long lastA = millis();
  // if (millis() - lastA > 100) {
  //   DEBUG_PRINTLN("SPP可读：" + String(avai));
  //   lastA = millis();
  // }

  if (avai) {
    //*1️1️1️1️== airRX → a2w环形Buf    ==1️1️1️1️*
    packetSize = SerialBT.available();
    int16_t first_part = checkRingSpace(packetSize);
    if (first_part >= 0) {
      SerialBT.readBytes(a2wRing + a2wHead, first_part);  // 第一段：head → 末尾
      if (a2wToWrite > first_part)
        SerialBT.readBytes(a2wRing, a2wToWrite - first_part);    // 第二段：绕回开头（如果需要）
      a2wHead = (a2wHead + a2wToWrite) & A2W_RING_SIZE_SUB_ONE;  // 更新 head
      packetSize = packetSize - a2wToWrite;  // 剩余未读字节，但其实留着没用，单纯是为了和air_udp对齐而已
      // DEBUG_PRINTLN(ringLack > 0    ? "填满，缺少：" + String(ringLack) + " B"
      //               : ringLack == 0 ? "正好填满"
      //                               : "填入，剩余：" + String(-ringLack) + " B");
    } else {
      // DEBUG_PRINTLN("填入前已满，缺少：" + String(ringLack) + " Byte");
    }
  }
  //*2️2️2️2️== a2w环形Buf → SerialTX ==2️2️2️2️*
  RingToSerial();  // 主循环高频调用，模拟流，每次写入serial fifo可用空间



  // MODE_PAYLOAD_MAX = SerialBT.availableForWrite();
  // static unsigned long lastT = millis();
  // if (millis() - lastT > 100) {
  //   DEBUG_PRINTLN("SPP可写：" + String(MODE_PAYLOAD_MAX));
  //   lastT = millis();
  // }
  // if (MODE_PAYLOAD_MAX > 0) {
  if (deviceConnected) {
    //*3️3️3️3️== SerialRX → w2a线性Buf ==3️3️3️3️*
    serialToW2aBuf();  // 主循环高频调用，每次读取serial fifo 已用空间
    //*4️4️4️4️== w2a线性Buf → AirTX    ==4️4️4️4️*
    if (shouldFlush()) {
      int len = SerialBT.write(w2aBuf, w2aLen);
      if (len > 0) {
        w2aLen = 0;
        lastFlushUs = micros();
      }
    }
  }
}
