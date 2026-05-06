#include "air_tcp.h"

WiFiServer tcpServer(0);
WiFiClient tcpClient;

void airTcp_begin() {
  DEBUG_PRINTLN("[TCP] 初始化...");
  deviceConnected = false;

  // 尝试连接STA，失败则启动AP
  if (!connectToSTA()) {
    startAPMode();
  }

  // 启动TCP服务器
  tcpServer.close();                       // 关掉占位的
  tcpServer = WiFiServer(ESP32_UDP_PORT);  // 用正确端口重建
  tcpServer.begin();
  tcpServer.setNoDelay(true);  // 重要！一定要关闭Nagle
  DEBUG_PRINTLN("[TCP] TCP服务器已启动，端口: " + String(ESP32_UDP_PORT));

  // 打印连接信息
  DEBUG_PRINTLN("\n========================================");
  DEBUG_PRINTLN("   ESP32 TCP 透传");
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
    DEBUG_PRINT(apTcp_fixed.SSID);
    DEBUG_PRINT("  密码: ");
    DEBUG_PRINTLN(apTcp_fixed.PassWord);
    DEBUG_PRINTLN("   本机IP: " AP_IP_STRING);
  }
  // 获取广播地址
  DEBUG_PRINTLN("\n 🔧协议: TCP");
}

void airTcp_update() {
  checkWifiConnection();  // 连接状态检查和重连逻辑
  // 1. 接受新连接
  if (tcpServer.hasClient()) {
    if (!tcpClient || !tcpClient.connected()) {
      tcpClient = tcpServer.available();
      tcpClient.setNoDelay(true);  // 重要！一定要关闭Nagle
      DEBUG_PRINTLN("[TCP] 新客户端连接: " + tcpClient.remoteIP().toString());
      tcpClientConnected = true;

      uint8_t tempBuf[128];
      S_DATA->read(tempBuf,
                   S_DATA->available());  // 连接时清空串口缓冲，防止旧数据干扰
      w2aLen = 0;                         // w2a线性buf归零
      a2wTail =
          a2wHead;  //  a2w环形Buf归零（注意head不归零，保持原位，tail追上head就表示空了）
      a2wToWrite = 0;          // a2w本次要写入的字节数归零
      packetSize = 0;          // air包大小归零
      ringLack = 0;            // 环形buf状态归零
      lastFlushUs = micros();  // flush计时器归零

    } else {
      // 已有客户端，拒绝新连接
      WiFiClient newClient = tcpServer.available();
      newClient.stop();
      DEBUG_PRINTLN("[TCP] 已有客户端，拒绝新连接");
    }
  }

  // 2. 监控连接状态
  if (tcpClient && !tcpClient.connected()) {
    DEBUG_PRINTLN("[TCP] 客户端断开连接");
    while (tcpClient.available()) tcpClient.flush();  // 清空TCP缓冲
    tcpClient.stop();
    tcpClientConnected = false;
  }

  // // AP模式处理DNS
  // if (isAPMode) dnsServer.processNextRequest();//发现域名劫持无效，所以注释了

  // ESP-IDF 默认值（lwIP 协议栈）
  // #define TCP_WND               (4 * TCP_MSS)  // 约 5744 字节
  // #define TCP_SND_BUF           (2 * TCP_MSS)  // 约 2872 字节
  // TCP RX 窗口本身就是环形缓冲区，没必要手动写了。

  //*1️1️1️1️== airRX → a2w环形Buf    ==1️1️1️1️*
  // 自动
  //*2️2️2️2️== a2w环形Buf → SerialTX ==2️2️2️2️*
  int16_t uartTxSpaceForWrite =
      S_DATA->availableForWrite();  // 交给主循环高频轮询，分批读
  if (uartTxSpaceForWrite > 0) {
    int len = tcpClient.read(
        a2wBuf, uartTxSpaceForWrite);  // fifo有多少空写入多少，不阻塞主循环
    if (len > 0) {
      S_DATA->write(a2wBuf, len);
    }
  }
  if (tcpClientConnected)  // 断连后没发完的Air包还是会发给Serial，但是Serial就没必要发给Air了
  {
    //*3️3️3️3️== SerialRX → w2a线性Buf ==3️3️3️3️*
    serialToW2aBuf();  // 主循环高频调用，每次读取serial fifo 已用空间
    //*4️4️4️4️== w2a线性Buf → AirTX    ==4️4️4️4️*
    if (shouldFlush()) {
      DEBUG_PRINTLN("[TCP] 发送 " + String(w2aLen) + " 字节到 " +
                    String(ESP32_UDP_PORT));
      tcpClient.write(w2aBuf, w2aLen);
      w2aLen = 0;
      lastFlushUs = micros();
    }
    // 主循环里会用yiled,且所有任务都有if和elapsed判断，主循环极快，所以轮询就够，不需要ISR，也不需要DMA
  }
}