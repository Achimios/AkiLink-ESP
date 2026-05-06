/**
 * ═══════════════════════════════════════════════════════════════
 *  buf_ops.h — 缓冲区操作（ESP-IDF 版）
 * ═══════════════════════════════════════════════════════════════
 *
 *  移植自原 Arduino 项目的 buf_rules.h / buf_rules.cpp
 *
 *  核心数据流：
 *    ┌──────────┐      ┌─────────┐      ┌──────────┐
 *    │ UART RX  │ ──→  │ w2aBuf  │ ──→  │ UDP TX   │
 *    │ (飞控)   │      │ (线性)  │      │ (空中)   │
 *    └──────────┘      └─────────┘      └──────────┘
 *
 *    ┌──────────┐      ┌─────────┐      ┌──────────┐
 *    │ UDP RX   │ ──→  │ a2wRing │ ──→  │ UART TX  │
 *    │ (空中)   │      │ (环形)  │      │ (飞控)   │
 *    └──────────┘      └─────────┘      └──────────┘
 *
 *  为什么用两种缓冲区？
 *    - w2aBuf (线性)：从串口读 → 积累 → 一次性发 UDP。
 *      因为 UDP 发送需要连续内存（pbuf），线性最合适。
 *    - a2wRing (环形)：UDP 回调写入 → 主循环读出 → 写串口。
 *      因为回调在 lwIP 线程，主循环在 app 线程，ring 天然适合
 *      单生产者-单消费者无锁模型。
 * ═══════════════════════════════════════════════════════════════
 */
#ifndef BUF_OPS_H
#define BUF_OPS_H

#include "app_config.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"   /* esp_timer_get_time() for should_flush() */

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════ Wire → Air 线性缓冲区 ══════════ */

/** 全局线性缓冲区：UART 读取暂存 → 等 flush 条件满足后发出 */
extern uint8_t  w2aBuf[W2A_BUF_SIZE];
/** 当前已填充的字节数 */
extern volatile size_t w2aLen;

/* ══════════ Air → Wire 环形缓冲区 ══════════ */

/**
 * 环形缓冲区本体
 *
 * 为什么用 volatile？
 *   a2wHead 由 lwIP 的回调线程写入
 *   a2wTail 由主循环读取
 *   volatile 防止编译器缓存寄存器里的旧值
 *
 * 为什么不用 mutex？
 *   单生产者(回调) + 单消费者(主循环) 的 ring buffer
 *   只要 head/tail 是原子更新的（32位对齐的 size_t 在 ESP32 上是原子的），
 *   就不需要锁。这也是你原来代码的做法，完全正确。
 */
extern uint8_t  a2wRing[A2W_RING_SIZE];
extern volatile size_t a2wHead;   /* 写入位置（UDP 回调写） */
extern volatile size_t a2wTail;   /* 读取位置（主循环读）   */

/* ══════════ Flush 时间戳 ══════════ */

/**
 * 上次 flush 的时间戳（微秒）
 * 用 esp_timer_get_time() 替代 Arduino 的 micros()
 */
extern int64_t lastFlushUs;

/* ══════════ 内联工具函数 ══════════ */

/**
 * 计算环形缓冲区已用空间
 *
 * 位运算优化：
 *   (head - tail) & MASK  等价于  (head - tail) % SIZE
 *   但是位运算在 ESP32 上约 4ns，模运算约 100ns
 *   前提：SIZE 必须是 2 的幂
 */
static inline size_t a2w_used(void) {
    return (A2W_RING_SIZE + a2wHead - a2wTail) & A2W_RING_MASK;
}

/** 计算环形缓冲区剩余空间（保留 1 字节区分满/空） */
static inline size_t a2w_free(void) {
    return A2W_RING_MASK - a2w_used();
    /* 注意：能用的最大空间 = SIZE-1，
     * 因为 head == tail 既表示空也表示满，
     * 所以牺牲 1 字节来区分这两种状态 */
}

/**
 * 检查环形缓冲区是否有足够空间写入 len 字节
 *
 * @param len     要写入的字节数（传入引用，负数会被修正为 0）
 * @param to_write [输出] 实际可写入的字节数（min(len, free)）
 * @return 第一段的字节数（head → 数组末尾），-1 表示无空间
 *
 * 为什么返回"第一段"？
 *   环形缓冲区写入可能需要 wrap around（回卷），
 *   第一段 = head 到数组末尾，第二段 = 数组开头到剩余
 *   调用者用两次 memcpy 完成写入
 */
static inline int16_t check_ring_space(int16_t len, size_t *to_write) {
    if (len <= 0) return -1;

    size_t free_space = a2w_free();
    if (free_space == 0) return -1;

    /* 实际写入量 = min(需要写, 空闲) */
    *to_write = (size_t)len < free_space ? (size_t)len : free_space;

    /* 第一段：从 head 到数组末尾的连续空间 */
    size_t first_part = A2W_RING_SIZE - a2wHead;
    if (first_part > *to_write) first_part = *to_write;
    return (int16_t)first_part;
}

/**
 * 判断是否该 flush（发送 w2aBuf 中的数据到 UDP）
 *
 * 策略：
 *   1) 至少有 FLUSH_MIN_SIZE 字节数据
 *   2) 距上次 flush 至少过了 FLUSH_MIN_TIME_US 微秒
 *   3) 满足以上两个"门槛"后，只要数据量 >= FLUSH_THRES_SIZE
 *      或时间 >= FLUSH_THRES_TIME_US，就立刻发
 *
 * 这个策略的好处：
 *   - 防止每个字节都发一个 UDP 包（开销太大）
 *   - 防止数据永远卡在缓冲区里不发（超时兜底）
 *   - 对 MAVLink：心跳包几十字节，1ms 内即可发出
 */
static inline bool should_flush(void) {
    int64_t now = esp_timer_get_time();              /* 微秒级时间戳 */
    int64_t elapsed = now - lastFlushUs;
    return (w2aLen >= FLUSH_MIN_SIZE)
        && (elapsed >= FLUSH_MIN_TIME_US)
        && ((int)w2aLen >= FLUSH_THRES_SIZE || elapsed >= FLUSH_THRES_TIME_US);
}

/* ══════════ 核心数据搬运函数 ══════════ */

/**
 * a2wRing → UART TX（Air → Wire 方向）
 *
 * 从环形缓冲区取数据，写入 UART 硬件发送队列。
 * 零阻塞：只写 UART TX FIFO 当前能吃下的量。
 * 如果数据 wrap around，分两次 uart_write_bytes。
 */
void ring_to_serial(void);

/**
 * UART RX → w2aBuf（Wire → Air 方向）
 *
 * 从 UART 硬件接收缓冲区读取数据，追加到 w2aBuf。
 * 读取量受限于 w2aBuf 剩余空间（防溢出）。
 * 参数 max_payload 限制单包最大有效载荷（= UDP_PAYLOAD_MAX）。
 */
void serial_to_w2a(int32_t max_payload);

/**
 * 初始化数据 UART 驱动
 * 替代 Arduino 的 Serial2.begin(baud)
 *
 * 使用 IDF uart_driver_install：
 *   - 安装中断驱动的 UART 驱动
 *   - 分配 RX/TX 软件缓冲区
 *   - 配置波特率、数据位、校验、停止位
 */
void uart_data_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BUF_OPS_H */
