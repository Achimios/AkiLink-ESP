#include "air_tcp.h"

#include "data_config.h"
#include "buf_rules.h"
#include "wifi_config.h"

// 全局变量定义
struct tcp_pcb *server_pcb = NULL; // 监听 PCB
struct tcp_pcb *client_pcb = NULL; // 当前连接的客户端 PCB

/**
 * @brief TCP 错误回调
 * 当连接发生异常断开（如重传超时、收到 RST）时被触发
 */
static void tcp_err_cb(void *arg, err_t err) {
    DEBUG_PRINTLN("[TCP] 错误发生，连接重置");
    client_pcb = NULL;
    tcpClientConnected = false;
}

/**
 * @brief TCP 接收回调 (核心：Air -> a2wRing)
 * 当有数据从网络到达时，由 lwIP 协议栈直接调用
 */
static err_t tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        // p 为空表示客户端主动关闭了连接 (FIN)
        DEBUG_PRINTLN("[TCP] 客户端主动断开");
        tcp_close(tpcb);
        client_pcb = NULL;
        tcpClientConnected = false;
        return ERR_OK;
    }

   
    struct pbuf *q = p; // 指向当前 pbuf 链表的指针
    while (q != NULL) { // 遍历 pbuf 链表（TCP 数据包可能由多个不连续的 pbuf 组成）
        packetSize = (int16_t)q->len; 
        int16_t first_part = checkRingSpace(packetSize);
        if (first_part >= 0) {
            // 零拷贝：直接从 pbuf 的 payload 拷贝到你的环形缓冲区
            memcpy(a2wRing + a2wHead, q->payload, first_part);
            if (a2wToWrite > first_part) 
                memcpy(a2wRing, (uint8_t*)q->payload + first_part, a2wToWrite - first_part);
            a2wHead = (a2wHead + a2wToWrite) & A2W_RING_SIZE_SUB_ONE;
            // 告诉协议栈：我已经成功“消费”了这些数据，你可以更新接收窗口了
            tcp_recved(tpcb, q->len);
        } else {
            DEBUG_PRINTLN("TCP 环形缓冲区已满，丢弃包"); // 注意：TCP 通常不建议丢包，但对于透传，如果缓冲区满了且你不想阻塞，只能丢弃
            break; // 环形缓冲区满了，后续数据不处理了（丢包），直接跳出循环，等主循环发完Serial后再来读TCP包
        }
        q = q->next; // 处理下一个 pbuf 节点
    }

    pbuf_free(p); // 释放 pbuf 链表，非常重要，防止内存泄漏
    return ERR_OK;
}

/**
 * @brief TCP 接受连接回调
 * 当有新客户端连接到监听端口时被触发
 */
static err_t tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
    if (client_pcb != NULL) {
        // 已经有一个客户端在连接，拒绝新连接
        DEBUG_PRINTLN("[TCP] 已有连接，拒绝新客户端");
        tcp_abort(newpcb);
        return ERR_ABRT;
    }

    DEBUG_PRINTLN("[TCP] 新客户端接入");
    client_pcb = newpcb;
    tcpClientConnected = true;

    // 配置新连接
    tcp_nagle_disable(client_pcb); // 禁用 Nagle 算法，实现极限响应速度
    tcp_arg(client_pcb, NULL);      // 我们可以传递自定义指针作为回调参数，这里传空
    tcp_recv(client_pcb, tcp_recv_cb); // 注册接收回调
    tcp_err(client_pcb, tcp_err_cb);   // 注册错误回调

    // 连接初始化：清空缓冲区逻辑保持不变
    w2aLen = 0;
    a2wTail = a2wHead;
    a2wToWrite = 0;
    packetSize = 0;
    ringLack = 0;
    lastFlushUs = micros();

    return ERR_OK;
}

void airTcp_begin() {
    DEBUG_PRINTLN("[TCP] 初始化 (lwIP Raw 模式)...");

    // WiFi 连接逻辑（保持你的原有逻辑）
    if (!connectToSTA()) {
        startAPMode();
    }

    // 创建监听 PCB
    server_pcb = tcp_new();
    if (server_pcb != NULL) {
        // 绑定所有本地 IP 上的指定端口
        err_t err = tcp_bind(server_pcb, IP_ADDR_ANY, ESP32_UDP_PORT);
        if (err == ERR_OK) {
            // 进入监听状态
            server_pcb = tcp_listen(server_pcb);
            // 注册接受新连接的回调
            tcp_accept(server_pcb, tcp_accept_cb);
            DEBUG_PRINTLN("[TCP] 服务器已在端口 " + String(ESP32_UDP_PORT) + " 监听");
        } else {
            DEBUG_PRINTLN("[TCP] 绑定失败");
        }
    } else {
        DEBUG_PRINTLN("[TCP] 创建 PCB 失败");
    }
}

void airTcp_update() {
    checkWifiConnection(); // 保持 WiFi 状态检查

    // 1. Air -> Wire (SerialTX) 
    // 在 lwIP Raw 模式下，数据接收由 tcp_recv_cb 自动处理，
    // 我们只需要在 update 里高频调用 RingToSerial 即可。
      //*2️2️2️2️== a2w环形Buf → SerialTX ==2️2️2️2️*
    RingToSerial(); 

    //没必要检查 tcpClientConnected 
    if (client_pcb != NULL) { //一旦对方断开连接，这个 pcb 指针可能会被协议栈释放或置空。如果你不检查就去 tcp_write(client_pcb, ...)，ESP32 会立刻重启（Panic）。
       //*3️3️3️3️== SerialRX → w2a线性Buf ==3️3️3️3️*
        serialToW2aBuf();

         //*4️4️4️4️== w2a线性Buf → AirTX    ==4️4️4️4️*
        if (shouldFlush()) {
            // 获取当前 TCP 发送缓冲区的可用大小
            uint16_t can_send = tcp_sndbuf(client_pcb);
            
            if (can_send >= w2aLen) {
                // 使用 tcp_write 提交数据。
                // TCP_WRITE_FLAG_COPY: 将数据拷贝到协议栈内部缓冲。
                // 虽然不是真正的“零拷贝发送”，但它是最稳妥的，因为 w2aBuf 会被立刻重用。
                err_t err = tcp_write(client_pcb, w2aBuf, w2aLen, TCP_WRITE_FLAG_COPY);
                if (err == ERR_OK) {
                    // 立即触发发送（类似于 flush）
                    tcp_output(client_pcb);
                    w2aLen = 0;
                    lastFlushUs = micros();
                }
            } else {
                // 发送缓冲区不足，等待下一轮 update 再尝试
                // 这就是 TCP 的自动流控起作用了
                DEBUG_PRINTLN("[TCP] 发送缓冲满，等待...");
            }
        }
    }
}
