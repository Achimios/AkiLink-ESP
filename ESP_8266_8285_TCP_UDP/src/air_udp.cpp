#include "air_udp.h"

// ============================================================
//  ESP8266 / ESP8285  air_udp.cpp
//  从 SRC_OLD 移植，Arduino WiFiUDP API — 和ESP32基本一致
//  改动: ESP32_UDP_PORT → ESP_UDP_PORT
//        apUdp_fixed.SSID → AP_NAME_UDP (去掉了ApSsidPwd结构体)
// ============================================================

WiFiUDP udp;

IPAddress udpBroadcastIP;
IPAddress udpTargetIP(0, 0, 0, 1);
IPAddress udpSendIP(0, 0, 0, 2);
int32_t target_port;

// 获取广播地址
static IPAddress getudpBroadcastIP() {
    if (isAPMode) {
        return IPAddress(AP_IP_BRDCST_COMMA);   // AP模式固定广播地址
    } else {
        // STA模式：根据当前IP和子网掩码计算广播地址
        IPAddress localIP    = WiFi.localIP();
        IPAddress subnetMask = WiFi.subnetMask();
        return IPAddress(
            localIP[0] | ~subnetMask[0],
            localIP[1] | ~subnetMask[1],
            localIP[2] | ~subnetMask[2],
            localIP[3] | ~subnetMask[3]
        );
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
    udp.begin(ESP_UDP_PORT);
    DEBUG_PRINTLN("[UDP] 监听端口: " + String(ESP_UDP_PORT)
                + "，目标端口: " + String(PC_UDP_PORT));

    // 打印连接信息
    DEBUG_PRINTLN("\n========================================");
    DEBUG_PRINTLN("   ESP8266 UDP 透传");
    DEBUG_PRINTLN("========================================");

    if (!isAPMode) {
        DEBUG_PRINTLN("   模式: STA（家庭WiFi）");
        DEBUG_PRINTLN("   本机IP: " + WiFi.localIP().toString());
    } else {
        DEBUG_PRINTLN("   模式: AP（热点）");
        DEBUG_PRINTLN("   SSID: " AP_NAME_UDP);
        DEBUG_PRINTLN("   密码: " AP_PWD);
        DEBUG_PRINTLN("   本机IP: " AP_IP_STRING);
    }

    udpBroadcastIP = getudpBroadcastIP();
    DEBUG_PRINTLN("   广播地址: " + udpBroadcastIP.toString());
    DEBUG_PRINTLN("========================================\n");
}


void airUdp_update() {

    checkWifiConnection();   // 连接状态检查和重连逻辑

    // ========== 1. UDP → 环形缓冲区 ==========
    if (packetSize <= 0)
        packetSize = udp.parsePacket();

    while (packetSize > 0 && a2wFree() > 0)
    // → 读当前包（没读完的话下个主循环不调用 parsePacket()）
    // → 读完了？取下一个包，直到 ring 满或 pbuf 空
    // RAM操作快，while 防止电脑发来一堆小包把 pbuf 链表塞满
    {
        // 首包配对 IP
        if (udpTargetIP[0] == 0) {
            udpTargetIP = udp.remoteIP();
            target_port = udp.remotePort();
            DEBUG_PRINTLN("🔗 配对 IP: " + udpTargetIP.toString()
                        + ":" + String(target_port));
        }

        // 来源IP过滤
        if (!LISTEN_BROAD && udp.remoteIP() != udpTargetIP) {
            packetSize = udp.parsePacket();
            DEBUG_PRINTLN("⚠️ 非配对 IP，丢 UDP 包");
            continue;
        }

        //*1️⃣== airRX → a2w环形Buf ==*
        int16_t first_part = checkRingSpace(packetSize);
        if (first_part >= 0) {
            udp.read(a2wRing + a2wHead, first_part);
            if (a2wToWrite > (size_t)first_part)
                udp.read(a2wRing, a2wToWrite - first_part);

            a2wHead = (a2wHead + a2wToWrite) & A2W_RING_SIZE_SUB_ONE;
            packetSize -= a2wToWrite;

            DEBUG_PRINTLN(ringLack > 0    ? "填满，缺少：" + String(ringLack) + " B"
                        : ringLack == 0   ? "正好填满"
                                          : "填入，剩余：" + String(-ringLack) + " B");
        } else {
            DEBUG_PRINTLN("填入前已满，缺少：" + String(ringLack) + " B");
        }

        if (packetSize <= 0)
            packetSize = udp.parsePacket();
    }

    //*2️⃣== a2w环形Buf → SerialTX ==*
    RingToSerial();

    if (deviceConnected)   // 断连后残留的Air包照常给Serial，但Serial没必要再发Air了
    {
        //*3️⃣== SerialRX → w2a线性Buf ==*
        serialToW2aBuf();

        //*4️⃣== w2a线性Buf → AirTX ==*
        if (shouldFlush()) {
            udpSendIP = SEND_BROAD ? udpBroadcastIP
                      : (udpTargetIP[0] == 0 ? udpBroadcastIP : udpTargetIP);

            DEBUG_PRINTLN("[UDP] 发送 " + String(w2aLen) + " B → "
                        + udpSendIP.toString() + ":" + String(target_port));

            udp.beginPacket(udpSendIP, target_port);
            udp.write(w2aBuf, w2aLen);
            udp.endPacket();

            w2aLen = 0;
            lastFlushUs = micros();
        }
    }
}
