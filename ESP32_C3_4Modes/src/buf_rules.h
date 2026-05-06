#pragma once
#define BUF_RULES_H
#include <Arduino.h>

#include "data_config.h"


// 🧑‍💻 缓冲区 🧑‍💻
//************* WIFI——和 UART 交互的 *************
#define uart_rx_soft_buf_size 1024 // 512   //串口接收软件缓冲区大小
#define w2aSize 1024               // 600   //wire to air 线性缓冲区，一次清空，积累，所以大点儿 // WIFI包大，所以现线性缓冲区大点儿能发大包

// 不需要专门为以下独立设置线性缓冲区大小，但是从serial 读取到缓冲区时会限制防止分片或阻塞
extern int32_t MODE_PAYLOAD_MAX;  // 当前模式最大有效载荷，根据模式动态设置
#define SPP_PAYLOAD_MAX 600 // SPP 最大有效载荷，超过会阻塞式分片
#define BLE_PAYLOAD_MAX 500// BLE MTU 限制，实际有效载荷更小（20-200字节不等）
#define UDP_PAYLOAD_MAX 1472 // UDP 最大有效载荷，超过会IP分片
//TCP 自带ringBuf 且够大，不需要严格限制，但建议分批写入（如 1KB 一次），防止瞬间挤爆 RAM

// - **UDP 最大 Payload:** $1500 - 20(IP头) - 8(UDP头) = \mathbf{1472}$ **字节**。
// - **TCP 最大 Payload:** $1500 - 20(IP头) - 20(TCP头) = \mathbf{1460}$ **字节**（这通常被称为 MSS - Maximum Segment Size）。

//共 fifo 128B + 软件 + 线性w2aSize
extern uint8_t w2aBuf[w2aSize];     // 5模式全局共享的wire to air缓冲区
extern volatile size_t w2aLen;      //w2a缓冲区当前大小





//************* TCP——和 AIR 交互的 *************
// #define uart_tx_soft_buf_size 256 //串口发送软件缓冲区大小 //Arduino 框架下serial tx soft buf不暴露
#define a2wSize 128 //air to wire 线性缓冲区，一次清空，不积累，等于serial tx fifo 大小
extern uint8_t a2wBuf[a2wSize];
//不积累，不需要a2wLen

//************* UDP——和 AIR 交互的 *************
// #define tempBufSize 600 // 和环形缓冲区交互的临时缓冲区
#define A2W_RING_SIZE 8192  //1024  // 环形缓冲区大小（建议大于UDP MAX PAYLOAD 1472)，因为无serial tx soft buf，所以需要手动创建。
// 共 fifo 128B + 环形A2W_RING_SIZE。// TCP自己有5744B不需要
#define A2W_RING_SIZE_SUB_ONE (A2W_RING_SIZE - 1) //环形缓冲区实际可用大小，因为 head==tail 时区分不了满和空，所以要浪费一个字节
//目前需要为2的幂，以便性能优化使用位运算代替取模。对于280B包，4ns vs 100ns
//其实无所谓，后面可以改成非2的幂，大小更灵活

// extern uint8_t tempBuf[tempBufSize];
extern uint8_t a2wRing[A2W_RING_SIZE];
extern volatile size_t a2wHead;  // 写入位置（AIR 写入）
extern volatile size_t a2wTail;  // 读取位置（Serial 读取）
extern int16_t packetSize; //当前AIR包大小
extern size_t a2wToWrite;
extern volatile int16_t ringLack; //环形buf缺少的字节数，负数表示剩余空间，正数表示超出部分（覆盖了旧数据）

//************* Flush 判断 *************
extern unsigned long lastFlushUs;  //距离上次wire to air flush的时间




// ========== 环形缓冲区定义 ==========

// 计算已用空间
inline size_t a2wUsed() {
    return (A2W_RING_SIZE + a2wHead - a2wTail ) & A2W_RING_SIZE_SUB_ONE;
    //                                          % A2W_RING_SIZE
    //     // ⚡性能优化提示⚡//   A2W_RING_SIZE必须是2的幂，这样就可以用位运算代替取模，快几十倍。对于280B包，4ns vs 100ns
}
// 计算剩余空间
inline size_t a2wFree() {
    return A2W_RING_SIZE_SUB_ONE - a2wUsed();  // -1 避免 head==tail 无法区分满还是空，所以比实际容量少1
}
// ⚡性能优化提示⚡
// 方式	                编译结果	                        调用开销
// 不用 inline	        普通函数调用	                    ~10-20ns（压栈、跳转、返回）
// inline 在 .cpp	    本文件可能内联，其他文件普通调用	  ~0-20ns
// inline 在 .h	        全部内联展开	                    ~0ns


// 写入环形缓冲区（从 AIR 读取后调用）
int16_t checkRingSpace(int16_t &len);
// 从环形缓冲区读取（写入 Serial 时调用）
// size_t readRing_writeToSerial(uint8_t* data, size_t maxLen);
// 环形缓冲区 到 临时缓冲区 到 串口
void RingToSerial() ;
// 串口 到 线性缓冲区
void getMaxPayLoad();
void serialToW2aBuf() ;
// 判断 Flush 条件
bool shouldFlush();

