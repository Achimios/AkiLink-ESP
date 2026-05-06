#pragma once
#define BUF_RULES_H
#include <Arduino.h>

#include "data_config.h"



// 🧑‍💻 缓冲区 🧑‍💻
//************* WIFI——和 UART 交互的 *************
#define s_dat_rx_sz 1024  // 串口接收软件缓冲区大小，因为 串口慢，其实也可以小点儿。但是防止BLE卡顿还是大点儿吧
#define s_dat_tx_sz 1024  // 串口发送软件缓冲区大小 ，有a2wRing了，这里小点儿
#define s_dbg_rx_sz 128   // 其实可以没有,防止底层调用失败多少给点
#define s_dbg_tx_sz 256   // 防止debug打印拖慢主进程

#define junk_size 1472
extern int16_t JUNK_SIZE;
#define w2aSize 1472
// 4的倍数，内存对齐      //wire to air 线性缓冲区，一次清空，积累，所以大点儿 //
// WIFI包大，所以现线性缓冲区大点儿能发大包
// 不需要专门为以下独立设置线性缓冲区大小，但是从serial 读取到缓冲区时会限制防止分片或阻塞
extern int32_t MODE_PAYLOAD_MAX;  // 当前模式最大有效载荷，根据模式动态设置
#define SPP_PAYLOAD_MAX 600       // SPP 最大有效载荷，超过会阻塞式分片
#define BLE_PAYLOAD_MAX 500       // BLE MTU 限制，实际有效载荷更小（20-200字节不等）
#define UDP_PAYLOAD_MAX 1472      // UDP 最大有效载荷，超过会IP分片
#define TCP_PAYLOAD_MAX 1000
// TCP 自带ringBuf 且够大，不需要严格限制，但建议分批写入（如 1KB 一次），防止瞬间挤爆 RAM

// - **UDP 最大 Payload:** $1500 - 20(IP头) - 8(UDP头) = \mathbf{1472}$ **字节**。
// - **TCP 最大 Payload:** $1500 - 20(IP头) - 20(TCP头) = \mathbf{1460}$ **字节**（这通常被称为 MSS - Maximum Segment
// Size）。

// 共 fifo 128B + 软件 + 线性w2aSize
extern uint8_t w2aBuf[w2aSize];  // 5模式全局共享的wire to air缓冲区
extern volatile size_t w2aLen;   // w2a缓冲区当前大小

// //************* TCP——和 AIR 交互的 *************
// // #define uart_tx_soft_buf_size 256 //串口发送软件缓冲区大小 //Arduino 框架下serial tx soft buf不暴露
// #define a2wSize 128 //air to wire 线性缓冲区，一次清空，不积累，等于serial tx fifo 大小
// extern uint8_t a2wBuf[a2wSize];
// //不积累，不需要a2wLen

//************* UDP——和 AIR 交互的 *************
// #define tempBufSize 600 // 和环形缓冲区交互的临时缓冲区
#define A2W_RING_SIZE \
  16384  // 2的次幂，便于 & 计算代替 %  // 环形缓冲区大小（建议大于UDP MAX PAYLOAD 1472)，因为无serial tx soft
                                // buf，所以需要手动创建。
// 共 fifo 128B + 环形A2W_RING_SIZE。// TCP自己有5744B不需要
#define A2W_RING_SIZE_SUB_ONE \
  (A2W_RING_SIZE - 1)  // 环形缓冲区实际可用大小，因为 head==tail 时区分不了满和空，所以要浪费一个字节
// 目前需要为2的幂，以便性能优化使用位运算代替取模。对于280B包，4ns vs 100ns
// 其实无所谓，后面可以改成非2的幂，大小更灵活

// extern uint8_t tempBuf[tempBufSize];
extern uint8_t a2wRing[A2W_RING_SIZE];
extern volatile size_t a2wHead;  // 写入位置（AIR 写入）
extern volatile size_t a2wTail;  // 读取位置（Serial 读取）
extern int16_t packetSize;       // 当前AIR包大小
extern size_t a2wToWrite;
extern volatile int16_t ringLack;  // 环形buf缺少的字节数，负数表示剩余空间，正数表示超出部分（覆盖了旧数据）

//************* Flush 判断 *************
extern unsigned long lastFlushUs;  // 距离上次wire to air flush的时间

// =================================================

// 计算已用空间
inline size_t a2wUsed() {
  return (A2W_RING_SIZE + a2wHead - a2wTail) & A2W_RING_SIZE_SUB_ONE;
  //                                          % A2W_RING_SIZE
  //     // ⚡性能优化提示⚡//   A2W_RING_SIZE必须是2的幂，这样就可以用位运算代替取模，快几十倍。对于280B包，4ns vs
  //     100ns
}
// 计算剩余空间
inline size_t a2wFree() {
  return A2W_RING_SIZE_SUB_ONE - a2wUsed();  // -1 避免 head==tail 无法区分满还是空，所以比实际容量少1
}

inline int16_t checkRingSpace(int16_t& len) {  // 引用
  if (len < 0) len = 0;  // 重复保险 //如果len小于0，说明上次包已经读完了，或者根本就没有包了，就不读了。
  if (len == 0) return -1;
  a2wToWrite = a2wFree();
  ringLack = len - a2wToWrite;                // 计算缺少的空间，正数表示超出部分（需要丢弃的旧数据），负数表示剩余空间
  if (a2wToWrite == 0) return -1;             // 如果没有空间了，就不读了。
  a2wToWrite = min((size_t)len, a2wToWrite);  // 如果len大，写入free大小，如果len小，写入len大小
  int16_t firstPart = min(a2wToWrite, A2W_RING_SIZE - a2wHead);  // 到末尾的空间
  return firstPart;
}

void RingToSerial();

// ========== W2A线性缓冲区定义 ==========
inline void getMaxPayLoad() {  //    放setup/init里
  if (crnt_Mode == MODE_BLE) {
    MODE_PAYLOAD_MAX = min(BLE_PAYLOAD_MAX, w2aSize);  // 限制为 500?，防止截断
  } else if (crnt_Mode == MODE_SPP) {
    MODE_PAYLOAD_MAX = min(SPP_PAYLOAD_MAX, w2aSize);  // 限制为 330? 以内，虽然防止 IP 分片时阻塞
                                                       // 没有availableForWrite让我判断，配合ringBuf。
  } else if (crnt_Mode == MODE_UDP) {
    MODE_PAYLOAD_MAX = min(UDP_PAYLOAD_MAX, w2aSize);  // 限制为 1472 以内，防止IP分片
  } else if (crnt_Mode == MODE_TCP) {
    MODE_PAYLOAD_MAX = min(TCP_PAYLOAD_MAX, w2aSize);  // TCP应用层为流式操作，不限制包大小。
  } else {
    MODE_PAYLOAD_MAX = w2aSize;  // 默认
  }
}

void serialToW2aBuf();

// 判断 Flush 条件
inline bool shouldFlush() {
  // 检查flush条件，因为无线协议都有包头，所以payload没必要太小
  unsigned long elapsedUs = micros() - lastFlushUs;
  // flush逻辑：防止数据卡住
  bool ok2Flush = w2aLen >= FLUSH_MIN_SIZE && elapsedUs >= FLUSH_MIN_TIME_US &&
                  (w2aLen >= FLUSH_THRES_SIZE || elapsedUs >= FLUSH_THRES_TIME_US);
  return ok2Flush;
}

inline bool shouldFlushJunk()  // 测试用，未调用会自动优化掉
{
  // 检查flush条件，因为无线协议都有包头，所以payload没必要太小
  unsigned long elapsedUs = micros() - lastFlushUs;
  // flush逻辑：防止数据卡住
  bool ok2Flush = elapsedUs >= FLUSH_THRES_TIME_US;
  return ok2Flush;
}

// 函数声明
void init_s_data_raw();   // 新增：底层串口初始化
void init_s_debug_raw();  // 新增：底层串口初始化



#ifdef _TIPS_
// 替代 memcpy
// 假设 buf_src 和 buf_dest 都是 4字节对齐，且 len 是 4 的倍数
void fast_copy(void* dest, void* src, size_t len) {
  uint32_t* d = (uint32_t*)dest;
  uint32_t* s = (uint32_t*)src;
  size_t count = len / 4;

  // CPU 流水线最喜欢的这种简单循环
  // 编译器开启 -O2 后，甚至会使用 SIMD 或特殊的块搬运指令
  while (count--) { *d++ = *s++; }
}
#endif
