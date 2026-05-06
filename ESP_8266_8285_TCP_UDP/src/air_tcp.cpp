#include "air_tcp.h"

// ============================================================
//  ESP8266 / ESP8285  air_tcp.cpp
//  从 SRC_OLD 移植，Arduino WiFiServer/WiFiClient
//  改动:
//    S_DATA-> (指针) → S_DATA. (宏=Serial)
//    apTcp_fixed.SSID → AP_NAME_TCP
//    a2wBuf → tcpA2wBuf (TCP专用小线性buf，因不走ring)
// ============================================================

// ⚠️ 不能 WiFiServer tcpServer(ESP_UDP_PORT) — 全局构造时 config 还没加载，port=0!
// 先用 0 占位，在 airTcp_begin() 中重新构造
WiFiServer tcpServer(0);
WiFiClient tcpClient;

// TCP Air→Wire 不走 a2wRing，用一个小线性缓冲区直通
// 大小 = ESP8266 TX FIFO (128B)，每次读多少取决于FIFO空位
#define TCP_A2W_BUF_SIZE 128
static uint8_t tcpA2wBuf[TCP_A2W_BUF_SIZE];


void airTcp_begin() {
    DEBUG_PRINTLN("[TCP] 初始化...");
    deviceConnected = false;

    // 尝试连接STA，失败则启动AP
    if (!connectToSTA()) startAPMode();

    // 启动TCP服务器 — 这里 config 已加载，ESP_UDP_PORT 有正确值
    tcpServer.close();                     // 关掉占位的
    tcpServer = WiFiServer(ESP_UDP_PORT);   // 用正确端口重建
    tcpServer.begin();
    tcpServer.setNoDelay(true);   // 关闭 Nagle，透传必须!
    DEBUG_PRINTLN("[TCP] 服务器已启动，端口: " + String(ESP_UDP_PORT));

    // 打印连接信息
    DEBUG_PRINTLN("\n========================================");
    DEBUG_PRINTLN("   ESP8266 TCP 透传");
    DEBUG_PRINTLN("========================================");

    if (!isAPMode) {
        DEBUG_PRINTLN("   模式: STA（家庭WiFi）");
        DEBUG_PRINTLN("   本机IP: " + WiFi.localIP().toString());
    } else {
        DEBUG_PRINTLN("   模式: AP（热点）");
        DEBUG_PRINTLN("   SSID: " AP_NAME_TCP);
        DEBUG_PRINTLN("   密码: " AP_PWD);
        DEBUG_PRINTLN("   本机IP: " AP_IP_STRING);
    }
    DEBUG_PRINTLN("   协议: TCP");
    DEBUG_PRINTLN("========================================\n");
}


void airTcp_update() {

    checkWifiConnection();

    // ===== 1. 接受新连接 =====
    if (tcpServer.hasClient()) {
        if (!tcpClient || !tcpClient.connected()) {
            tcpClient = tcpServer.accept(); //tcpServer.available()
            tcpClient.setNoDelay(true);   // 客户端也要关 Nagle!
            DEBUG_PRINTLN("[TCP] 新客户端: " + tcpClient.remoteIP().toString());
            tcpClientConnected = true;

            // 新连接时清空所有缓冲区，防止旧数据串扰
            uint8_t junk[128];
            while (S_DATA.available() > 0) S_DATA.read(junk, sizeof(junk));  // 清空串口RX
            w2aLen = 0;
            lastFlushUs = micros();

        } else {
            // 已有客户端，拒绝新连接
            WiFiClient rejected = tcpServer.accept(); //tcpServer.available()
            rejected.stop();
            DEBUG_PRINTLN("[TCP] 已有客户端，拒绝新连接");
        }
    }

    // ===== 2. 监控连接状态 =====
    if (tcpClient && !tcpClient.connected()) {
        DEBUG_PRINTLN("[TCP] 客户端断开");
        tcpClient.flush();
        tcpClient.stop();
        tcpClientConnected = false;
    }

    // ===== 3. Air → Wire: TCP RX → Serial TX =====
    // TCP 自带 lwIP RX 窗口 (~5744B)，不需要 a2wRing
    // 每次读 min(tcpClient.available(), Serial TX FIFO 空位)
    // 直接推到硬件FIFO，完全非阻塞
    {
        int txFree = S_DATA.availableForWrite();   // TX FIFO 空位 (max 128)
        if (txFree > 0 && tcpClient && tcpClient.connected()) {
            int canRead = min(txFree, (int)TCP_A2W_BUF_SIZE);
            int len = tcpClient.read(tcpA2wBuf, canRead);
            if (len > 0) {
                S_DATA.write(tcpA2wBuf, len);
            }
        }
    }

    // ===== 4. Wire → Air: Serial RX → TCP TX =====
    if (tcpClientConnected) {
        //*3️⃣== SerialRX → w2a线性Buf ==*
        serialToW2aBuf();

        //*4️⃣== w2a线性Buf → AirTX ==*
        if (shouldFlush()) {
            DEBUG_PRINTLN("[TCP] 发送 " + String(w2aLen) + " B");
            tcpClient.write(w2aBuf, w2aLen);
            w2aLen = 0;
            lastFlushUs = micros();
        }
    }
}
