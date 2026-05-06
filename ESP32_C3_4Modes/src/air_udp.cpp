#include "air_udp.h"

// 全局变量定义
WiFiUDP udp;

IPAddress udpBroadcastIP;
IPAddress udpTargetIP(0, 0, 0, 1);
IPAddress udpSendIP(0, 0, 0, 2);
int32_t target_port;

// 获取广播地址函数
static IPAddress getudpBroadcastIP() {
  if (isAPMode) {
    return IPAddress(AP_IP_BRDCST_COMMA);  // AP模式固定广播地址
  } else {
    // STA模式：根据当前IP和子网掩码计算广播地址
    IPAddress localIP = WiFi.localIP();
    IPAddress subnetMask = WiFi.subnetMask();
    return IPAddress(localIP[0] | ~subnetMask[0], localIP[1] | ~subnetMask[1], localIP[2] | ~subnetMask[2],
                     localIP[3] | ~subnetMask[3]);
  }
}

// ==================== 核心函数 ====================
void airUdp_begin() {
  DEBUG_PRINTLN("[UDP] 初始化...");
  deviceConnected = false;
  // 尝试连接STA，失败则启动AP
  if (!connectToSTA()) startAPMode();


  // 启动UDP服务器
  target_port = PC_UDP_PORT;
  udp.begin(ESP32_UDP_PORT);  // 监听端口
  DEBUG_PRINTLN("[UDP] UDP服务器已启动，监听端口: " + String(ESP32_UDP_PORT) + "，目标端口: " + String(PC_UDP_PORT));

  // 打印连接信息
  DEBUG_PRINTLN("\n========================================");
  DEBUG_PRINTLN("   ESP32 UDP 透传");
  DEBUG_PRINTLN("========================================");

  // 打印WIFI信息
  if (!isAPMode) {
    // STA模式
    DEBUG_PRINTLN("   模式: STA（家庭WiFi）");
    DEBUG_PRINTLN("   本机IP: " + WiFi.localIP().toString());
  } else {
    // AP模式
    DEBUG_PRINTLN("   模式: AP（热点）");
    DEBUG_PRINT("SSID: ");
    DEBUG_PRINT(apUdp_fixed.SSID);
    DEBUG_PRINT("  密码: ");
    DEBUG_PRINTLN(apUdp_fixed.PassWord);
    DEBUG_PRINTLN("   本机IP: " AP_IP_STRING);
  }
  // 获取广播地址
  udpBroadcastIP = getudpBroadcastIP();
  DEBUG_PRINTLN("   广播地址: " + udpBroadcastIP.toString());
  DEBUG_PRINTLN("\n 🔧协议: UDP");
  DEBUG_PRINTLN("========================================\n");
}

void airUdp_update() {

  checkWifiConnection();  // 连接状态检查和重连逻辑

  // AP模式处理DNS
  // if (isAPMode) dnsServer.processNextRequest(); //发现域名劫持无效，所以注释了

  // ========== 1. UDP → 环形缓冲区 ==========
  if (packetSize <= 0)               // 上次的读完了
    packetSize = udp.parsePacket();  // 传入下一个udp包

  while (packetSize > 0 && a2wFree() > 0)
  // → 读当前包（没读完的话下个主循环不调用udp.parsePacket()）
  // → 读完了？取下一个包 直到 ring 满或 pbuf 空
  // RAM操作很快，用while就是为了防止 电脑发来一堆 小包把 pbuf链表塞满了
  {

    if (udpTargetIP[0] == 0) {
      udpTargetIP = udp.remoteIP();  // 记录第一个发送者 IP 为配对 IP
      target_port = udp.remotePort();
      DEBUG_PRINTLN("🔗 配对 IP 锁定: " + udpTargetIP.toString() + ":" + String(target_port));
    }

    // 来源IP 过滤
    if (!crnt_wifiIpPort.listenBroad && udp.remoteIP() != udpTargetIP) {
      packetSize = udp.parsePacket();
      DEBUG_PRINTLN("⚠️ 非配对 IP，丢 UDP 包");
      continue;  // ← 跳过下面的写入逻辑，直接回到 while 判断
    }

    //*1️1️1️1️== airRX → a2w环形Buf    ==1️1️1️1️*
    int16_t first_part = checkRingSpace(packetSize);
    if (first_part >= 0) {
      udp.read(a2wRing + a2wHead, first_part);                                  // 第一段：head → 末尾
      if (a2wToWrite > first_part) udp.read(a2wRing, a2wToWrite - first_part);  // 第二段：绕回开头（如果需要）
      a2wHead = (a2wHead + a2wToWrite) & A2W_RING_SIZE_SUB_ONE;                 // 更新 head
      packetSize = packetSize - a2wToWrite;  // 更新 len 为剩余未写入的数据量，如果len小，则为负数。如果发完了，则为0。
      DEBUG_PRINTLN(ringLack > 0    ? "填满，缺少：" + String(ringLack) + " B"
                    : ringLack == 0 ? "正好填满"
                                    : "填入，剩余：" + String(-ringLack) + " B");
    } else {
      DEBUG_PRINTLN("填入前已满，缺少：" + String(ringLack) + " Byte");
    }

    if (packetSize <= 0)               // 上次的读完了
      packetSize = udp.parsePacket();  // 传入下一个udp包
  }

  //*2️2️2️2️== a2w环形Buf → SerialTX ==2️2️2️2️*
  RingToSerial();  // 主循环高频调用，模拟流，每次写入serial fifo可用空间

  if (deviceConnected)  // 断连后没发完的Air包还是会发给Serial，但是Serial就没必要发给Air了
  {
    //*3️3️3️3️== SerialRX → w2a线性Buf ==3️3️3️3️*
    serialToW2aBuf();  // 主循环高频调用，每次读取serial fifo 已用空间

    //*4️4️4️4️== w2a线性Buf → AirTX    ==4️4️4️4️*
    if (shouldFlush()) {
      udpSendIP = SEND_BROAD ? udpBroadcastIP : (udpTargetIP[0] == 0 ? udpBroadcastIP : udpTargetIP);
      DEBUG_PRINTLN("[UDP] 发送 " + String(w2aLen) + " B 到 " + udpSendIP.toString() + ":" + String(target_port));
      udp.beginPacket(udpSendIP, target_port);

      udp.write(w2aBuf, w2aLen);
      udp.endPacket();

      // 重置状态
      w2aLen = 0;
      lastFlushUs = micros();
    }
  }
}