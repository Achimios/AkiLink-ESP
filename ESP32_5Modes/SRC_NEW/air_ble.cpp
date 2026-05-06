#include "air_ble.h"

NimBLECharacteristic *txChar;
NimBLEServer *pServer;

void bleReconnect() {
  DEBUG_PRINTLN("已断线，重新开始广播BLE...");
  pServer->startAdvertising();
}


#ifdef _DRAFT
== == == == == == == == == == == == == == == == == == == == == == == == =
🚀 服务端回调：处理连接、MTU 和 速度协商 class ServerCallbacks : public NimBLEServerCallbacks
{
  void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) override
  {
    deviceConnected = true;

    // 1. 获取当前协商的 MTU
    uint16_t mtu = pServer->getPeerMTU(desc->conn_handle);
    MODE_PAYLOAD_MAX = mtu - 3;

    // 2. ⚡️ 请求极速模式！(Min Interval 6*1.25=7.5ms, Max 32*1.25=40ms, Latency 0, Timeout 400ms)
    // 这会让手机尽可能快地发送数据，大幅提升吞吐量
    pServer->updateConnParams(desc->conn_handle, 6, 32, 0, 400);

    DEBUG_PRINTLN("[BLE] 已连接 ID: " + String(desc->conn_handle));
    DEBUG_PRINTLN("[BLE] MTU协商结果: " + String(mtu) + " -> Payload: " + String(MODE_PAYLOAD_MAX));
  }

  void onDisconnect(NimBLEServer *pServer) override
  {
    deviceConnected = false;
    MODE_PAYLOAD_MAX = BLE_PAYLOAD_MAX; // 恢复默认保守值
    DEBUG_PRINTLN("[BLE] 已断开");
  }
};
#endif

class ServerCallbacks : public NimBLEServerCallbacks
{
  // ⚠️ 修正1: 签名变更为 (pServer, connInfo)
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override
  {
    deviceConnected = true;

    // 获取句柄和MTU
    uint16_t connHandle = connInfo.getConnHandle();
    uint16_t mtu = pServer->getPeerMTU(connHandle);

    MODE_PAYLOAD_MAX = mtu - 3;

    // ⚡️ 请求极速模式 (NimBLE v2 写法)
    pServer->updateConnParams(connHandle, 6, 32, 0, 400);

    DEBUG_PRINTLN("[BLE] 已连接 ID: " + String(connHandle));
    DEBUG_PRINTLN("[BLE] MTU: " + String(mtu));
  }

  // ⚠️ 修正2: 签名变更为 (pServer, connInfo, reason)
  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override
  {
    deviceConnected = false;
    MODE_PAYLOAD_MAX = BLE_PAYLOAD_MAX;
    DEBUG_PRINTLN("[BLE] 已断开, 原因码: " + String(reason));
  }
};

// =================================================
// 📥 接收回调：处理手机发来的数据
#ifdef _DRAFT
class RxCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *c) override
  {
    // NimBLE getValue 返回的是引用，无需拷贝，速度极快
    if (c->getValue().length() == 0)
      return;

    const uint8_t *data = c->getValue().data();
    packetSize = c->getValue().length();

    // *1️1️1️1️== airRX → a2w环形Buf ==1️1️1️1️*
    int16_t first_part = checkRingSpace(packetSize);
    if (first_part >= 0)
    {
      memcpy(a2wRing + a2wHead, data, first_part);
      if (a2wToWrite > first_part)
        memcpy(a2wRing, data + first_part, a2wToWrite - first_part);

      a2wHead = (a2wHead + a2wToWrite) & A2W_RING_SIZE_SUB_ONE;

      // 调试日志 (注意：高频数据时建议注释掉日志以提升性能)
      // DEBUG_PRINTLN("RX len:" + String(packetSize));
    }
    else
    {
      DEBUG_PRINTLN("[BLE] RingBuf Full! 丢弃: " + String(packetSize));
    }

    // 在 NimBLE 中，数据是库内部管理的，无需这里手动清空 packetSize
  }
};
#endif

class RxCallbacks : public NimBLECharacteristicCallbacks
{
  // ⚠️ 修正3: 签名变更为 (characteristic, connInfo)
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &connInfo) override
  {
    // 直接判断长度，如果为0直接跳过
    if (c->getValue().length() == 0)
      return;

    // 🔥 真正的零拷贝：直接拿指针，不生成 std::string
    const uint8_t *data = c->getValue().data();
    packetSize = c->getValue().length();

    // *1️1️1️1️== airRX → a2w环形Buf ==1️1️1️1️*
    int16_t first_part = checkRingSpace(packetSize);
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

// =================================================
#ifdef _DRAFT
void airBle_begin()
{
  DEBUG_PRINTLN("ESP32 BLE Serial (NimBLE Optimized)");

  NimBLEDevice::init(ESP_BLE_NAME);

  // ⚡️ 设置最大发射功率 (ESP_PWR_LVL_P9 是 +9dBm)
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  // ⚡️ 预设期望的 MTU 为 517 (最大值)，让 NimBLE 自动去协商
  NimBLEDevice::setMTU(517);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService *service = pServer->createService(SERVICE_UUID);

  // 创建 RX 特征 (手机写)
  NimBLECharacteristic *rxChar = service->createCharacteristic(
      CHAR_UUID_RX,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rxChar->setCallbacks(new RxCallbacks());

  // 创建 TX 特征 (手机读/通知)
  // 💡 NimBLE 自动管理 CCCD (2902)，不需要手动 addDescriptor，非常省事！
  txChar = service->createCharacteristic(
      CHAR_UUID_TX,
      NIMBLE_PROPERTY::NOTIFY);

  service->start();

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  adv->start();

  DEBUG_PRINTLN("BLE Listening...");
}
#endif

void airBle_begin()
{
  DEBUG_PRINTLN("ESP32 BLE Serial (NimBLE v2)");

  NimBLEDevice::init(ESP_BLE_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setMTU(517);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService *service = pServer->createService(SERVICE_UUID);

  NimBLECharacteristic *rxChar = service->createCharacteristic(
      CHAR_UUID_RX,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rxChar->setCallbacks(new RxCallbacks());

  txChar = service->createCharacteristic(
      CHAR_UUID_TX,
      NIMBLE_PROPERTY::NOTIFY);

  service->start();

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);

  // ⚠️ 修正4: 删除 setScanResponse(true);
  // NimBLE 会自动处理。如果你需要专门的 ScanResponse 数据，需要创建 NimBLEAdvertisementData 并调用 setScanResponseData
  // 这里保持默认即可。

  adv->start();

  DEBUG_PRINTLN("BLE Listening...");
}

void airBle_update()
{
  // -------- 断线重连逻辑 --------

  // *2️2️2️2️== a2w环形Buf → SerialTX ==2️2️2️2️*
  RingToSerial();

  if (!deviceConnected)
    return;

  // *3️3️3️3️== SerialRX → w2a线性Buf ==3️3️3️3️*
  serialToW2aBuf();

  // *4️4️4️4️== w2a线性Buf → AirTX ==4️4️4️4️*
  if (shouldFlush())
  {
    // NimBLE 支持直接传入 buffer 和 length，无需转 String，效率最高
    txChar->setValue(w2aBuf, w2aLen);
    txChar->notify();

    w2aLen = 0;
    lastFlushUs = micros();
  }
}