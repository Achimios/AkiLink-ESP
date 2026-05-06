// air_ble.cpp
#include "air_ble.h"

BLECharacteristic *txChar;
BLEServer *pServer;

// =================================================
class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *) override
  {
    deviceConnected = true;
    DEBUG_PRINTLN("[BLE] Connected");
  }

  void onDisconnect(BLEServer *) override
  {
    deviceConnected = false;
    MODE_PAYLOAD_MAX = BLE_PAYLOAD_MAX; // 重置
    DEBUG_PRINTLN("[BLE] Disconnected");
  }
};

// class RxCallbacks : public BLECharacteristicCallbacks {
//   void onWrite(BLECharacteristic* c) override {
//     std::string v = c->getValue();
//     //最新版board会报错，得把std::string改成std::string v = c->getValue().c_str();
//     //但这又会导致旧版本编译失败，所以只能在platformio.ini里指定Core版本了
//     if (!v.empty()) {
//       S_DATA->write((uint8_t*)v.data(), v.length());
//     }
//     //会阻塞，这里写入环形缓冲区，写入uart丢主循环
//   }
// };

// BLE 4.2 实际吞吐：~10-25 KB/s（理论 100KB/s）
// BLE 5.0 实际吞吐：~50-100 KB/s

// Serial @ 115200：11.5 KB/s
// Serial @ 460800：46 KB/s
// Serial @ 921600：92 KB/s

// BLE 4.2 @ 20KB/s vs Serial 460800 @ 46KB/s
// → BLE 远慢于 Serial！Serial 永远消化得完！

class RxCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *c) override
  {
    // std::string v = c->getValue();     //必须创建临时buf
    const std::string &v = c->getValue(); // 使用 const 引用，不會產生數據複製
    if (v.empty())
      return;

    const uint8_t *data = (const uint8_t *)v.data();
    packetSize = v.length();

    //*1️1️1️1️== airRX → a2w环形Buf    ==1️1️1️1️*
    int16_t first_part = checkRingSpace(packetSize); // ring 满了就丢弃多余部分。（BLE 速率慢，所以几乎不会发生)
    if (first_part >= 0)
    {
      memcpy(a2wRing + a2wHead, data, first_part);
      if (a2wToWrite > first_part)
        memcpy(a2wRing, data + first_part, a2wToWrite - first_part);
      a2wHead = (a2wHead + a2wToWrite) & A2W_RING_SIZE_SUB_ONE;
      DEBUG_PRINTLN(ringLack > 0 ? "填满，丢弃：" + String(ringLack) + " B" : ringLack == 0 ? "正好填满"
                                                                                            : "填入，剩余：" + String(-ringLack) + " B");
    }
    else
    {
      DEBUG_PRINTLN("填入前已满，丢弃：" + String(ringLack) + " Byte");
    }
    // packetSize = 0; // v.data()没法留着等下次调用，所以只能丢弃。。。
  }
};

// // ESP-IDF 底层回调，捕获 MTU 协商事件  🤝·🤝·🤝 MTU ·🤝·🤝·🤝
// static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
// {
//   if (event == ESP_GATTS_MTU_EVT)
//   {
//     MODE_PAYLOAD_MAX = param->mtu.mtu - 3;
//     DEBUG_PRINTLN("BLE最大有效载荷: " + String(MODE_PAYLOAD_MAX));
//   }
// }

void airBle_begin()
{
  DEBUG_PRINTLN("ESP32 BLE Serial (RAW)");

  BLEDevice::init(ESP_BLE_NAME);

  // // 注册底层回调，捕获 MTU 事件 🤝·🤝·🤝 MTU ·🤝·🤝·🤝
  // esp_ble_gatts_register_callback(gatts_event_handler); // ⚠️ 可能和 Arduino 库冲突！ 真冲突了，先注释

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *service = pServer->createService(SERVICE_UUID);

  BLECharacteristic *rxChar = service->createCharacteristic(
      CHAR_UUID_RX,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rxChar->setCallbacks(new RxCallbacks());

  txChar = service->createCharacteristic(
      CHAR_UUID_TX,
      BLECharacteristic::PROPERTY_NOTIFY);
  txChar->addDescriptor(new BLE2902());

  service->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->start();

  DEBUG_PRINTLN("BLE advertising...");
  DEBUG_PRINTLN(ESP_GATTS_MTU_EVT);
  // DEBUG_PRINTLN(ESP_SPP_CONG_EVT);
}

void airBle_update()
{
  // -------- 断线重广播--------
  if (!deviceConnected && oldDeviceConnected)
  {
    pServer->startAdvertising();
    DEBUG_PRINTLN("已断线，重新开始广播BLE...");
  }

  //*2️2️2️2️== a2w环形Buf → SerialTX ==2️2️2️2️*
  RingToSerial(); // 主循环高频调用，模拟流，每次写入serial fifo可用空间

  if (deviceConnected)
  { // 只在有连接时更新手动w2a缓存

    //*3️3️3️3️== SerialRX → w2a线性Buf ==3️3️3️3️*
    serialToW2aBuf(); // 主循环高频调用，每次读取serial fifo 已用空间
    
    //*4️4️4️4️== w2a线性Buf → AirTX    ==4️4️4️4️*
    if (shouldFlush())
    {
      txChar->setValue(w2aBuf, w2aLen);
      txChar->notify();

      w2aLen = 0;
      lastFlushUs = micros();
    }
  }
}