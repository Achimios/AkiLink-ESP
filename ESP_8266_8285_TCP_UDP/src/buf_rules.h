#pragma once
#define BUF_RULES_H
#include <Arduino.h>
#include "data_cfg.h"

// ============================================================
//  ESP8266 / ESP8285  buf_rules.h
//  Ported from ESP32 — 去掉 IDF uart_* API，改用 Arduino Serial
// ============================================================


// 🧑‍💻 串口缓冲区 🧑‍💻
// ESP8266 Arduino：
//   Serial RX 软件缓冲区 默认256B，可通过 setRxBufferSize() 扩大（需在 begin 之前调用）
//   Serial TX 无软件缓冲区，只有128B硬件FIFO
//   Serial1 仅TX（GPIO2），128B硬件FIFO
#define s_dat_rx_sz 1024  // Serial(UART0) RX 软件缓冲区大小


// 🧑‍💻 W2A 线性缓冲区 (Wire → Air) 🧑‍💻
// 串口读入 → 攒到 w2aBuf → 达到flush条件后一次性发往WiFi
#define w2aSize 1472   // 4的倍数，对齐。≥ UDP_PAYLOAD_MAX
extern uint8_t w2aBuf[w2aSize];
extern volatile size_t w2aLen;

// 各模式最大有效载荷
extern int32_t MODE_PAYLOAD_MAX;
#define UDP_PAYLOAD_MAX 1472   // 1500 - 20(IP) - 8(UDP) = 1472
#define TCP_PAYLOAD_MAX 1000   // TCP 流式, 不严格限制，分批写防挤爆RAM
// ESP8266 不支持 SPP / BLE，无需对应 PAYLOAD_MAX


// 🧑‍💻 A2W 环形缓冲区 (Air → Wire) 🧑‍💻
// WiFi收到数据 → 写入 a2wRing → RingToSerial() 推送到串口TX FIFO
// ESP8266 RAM有限(~50KB可用)，比ESP32的16KB缩到4KB
// 4096 = 2^12，可容纳 ~2.7个满载UDP包，对透传够用
#define A2W_RING_SIZE       4096
#define A2W_RING_SIZE_SUB_ONE (A2W_RING_SIZE - 1)  // head==tail 区分满/空，浪费1字节

extern uint8_t  a2wRing[A2W_RING_SIZE];
extern volatile size_t a2wHead;     // 写入位置 (Air写入)
extern volatile size_t a2wTail;     // 读取位置 (Serial读取)
extern int16_t  packetSize;         // 当前AIR包大小
extern size_t   a2wToWrite;
extern volatile int16_t ringLack;   // 正=溢出丢失量, 负=剩余空间


// 🧑‍💻 Flush 判断 🧑‍💻
extern unsigned long lastFlushUs;


// =================================================
//  内联函数
// =================================================

// 环形缓冲区已用空间
inline size_t a2wUsed() {
    return (A2W_RING_SIZE + a2wHead - a2wTail) & A2W_RING_SIZE_SUB_ONE;
    // ⚡ A2W_RING_SIZE 必须是2的幂，位运算代替取模
}

// 环形缓冲区剩余空间
inline size_t a2wFree() {
    return A2W_RING_SIZE_SUB_ONE - a2wUsed();  // -1 避免 head==tail 歧义
}

// 检查ring是否够写入len字节，返回第一段可写长度，-1表示无空间
inline int16_t checkRingSpace(int16_t& len) {
    if (len < 0) len = 0;
    if (len == 0) return -1;
    a2wToWrite = a2wFree();
    ringLack = len - a2wToWrite;
    if (a2wToWrite == 0) return -1;
    a2wToWrite = min((size_t)len, a2wToWrite);
    int16_t firstPart = min(a2wToWrite, A2W_RING_SIZE - a2wHead);
    return firstPart;
}


// ========== W2A: 设置当前模式最大载荷 ==========
inline void getMaxPayLoad() {
    switch (crnt_Mode) {
        case MODE_UDP: MODE_PAYLOAD_MAX = min(UDP_PAYLOAD_MAX, w2aSize); break;
        case MODE_TCP: MODE_PAYLOAD_MAX = min(TCP_PAYLOAD_MAX, w2aSize); break;
        default:       MODE_PAYLOAD_MAX = w2aSize; break;
    }
}


// ========== Flush 条件判断 ==========
inline bool shouldFlush() {
    unsigned long elapsedUs = micros() - lastFlushUs;
    return w2aLen >= FLUSH_MIN_SIZE
        && elapsedUs >= FLUSH_MIN_TIME_US
        && (w2aLen >= FLUSH_THRES_SIZE || elapsedUs >= FLUSH_THRES_TIME_US);
}


// =================================================
//  函数声明
// =================================================
void RingToSerial();      // a2wRing → Serial TX FIFO (非阻塞)
void serialToW2aBuf();    // Serial RX → w2aBuf (非阻塞)
void init_s_data();       // 初始化数据串口 Serial(UART0)
void init_s_debug();      // 初始化调试串口 Serial1(UART1)，条件性
