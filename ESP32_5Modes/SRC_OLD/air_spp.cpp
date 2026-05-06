#include "air_spp.h"

BluetoothSerial SerialBT;

void airSpp_begin()
{
  btStop();                     // 关闭 BLE
  SerialBT.begin(ESP_SPP_NAME); // SPP
  
  DEBUG_PRINTLN("ESP32 SPP Bridge (Buffered)");
  //   DEBUG_PRINTLN(ESP_GATTS_MTU_EVT);
  //   DEBUG_PRINTLN(ESP_SPP_CONG_EVT);
  //   获得 SPP 缓冲区大小
}

void airSpp_update()
{
  // ---------- Client check ----------
  if (!SerialBT.hasClient())
  {
    w2aLen = 0;
    yield(); // 主循环末尾已经有yield。这里还有必要加吗？应该有，因为后面有return。或者没有？因为退出的是airSpp_update()而非主循环。
    return;
  }
  
  if (SerialBT.available())
  {
    
    //*1️1️1️1️== airRX → a2w环形Buf    ==1️1️1️1️*
    packetSize = SerialBT.available();
    int16_t first_part = checkRingSpace(packetSize);
    if (first_part >= 0)
    {
      SerialBT.readBytes(a2wRing + a2wHead, first_part); // 第一段：head → 末尾
      if (a2wToWrite > first_part)
      SerialBT.readBytes(a2wRing, a2wToWrite - first_part);   // 第二段：绕回开头（如果需要）
      a2wHead = (a2wHead + a2wToWrite) & A2W_RING_SIZE_SUB_ONE; // 更新 head
      packetSize = packetSize - a2wToWrite;                     // 剩余未读字节，但其实留着没用，单纯是为了和air_udp对齐而已
      DEBUG_PRINTLN(ringLack > 0 ? "填满，缺少：" + String(ringLack) + " B" : ringLack == 0 ? "正好填满"
      : "填入，剩余：" + String(-ringLack) + " B");
    }
    else
    {
      DEBUG_PRINTLN("填入前已满，缺少：" + String(ringLack) + " Byte");
    }
  }
  //*2️2️2️2️== a2w环形Buf → SerialTX ==2️2️2️2️*
  RingToSerial(); // 主循环高频调用，模拟流，每次写入serial fifo可用空间
  
 MODE_PAYLOAD_MAX = SerialBT.availableForWrite();
  if (MODE_PAYLOAD_MAX > 0)
  {
    //*3️3️3️3️== SerialRX → w2a线性Buf ==3️3️3️3️*
    serialToW2aBuf(); // 主循环高频调用，每次读取serial fifo 已用空间
    //*4️4️4️4️== w2a线性Buf → AirTX    ==4️4️4️4️*
    if (shouldFlush())
    {
      SerialBT.write(w2aBuf, w2aLen);
    }
  }
}
