/**
 * ═══════════════════════════════════════════════════════════════
 *  app_config.h — 全局配置头（ESP-IDF 版）
 * ═══════════════════════════════════════════════════════════════
 *
 *  本文件集中定义：
 *    1) 硬件引脚 & UART 编号
 *    2) 缓冲区尺寸常量
 *    3) Wi-Fi AP/STA 默认参数
 *    4) UDP 端口 & Flush 策略参数
 *    5) 调试日志宏（基于 ESP_LOG）
 *
 *  设计原则：
 *    - 用 #define / constexpr 代替 Arduino 的全局对象
 *    - 所有可调参数集中在此，便于 menuconfig 之外的快速微调
 *    - 不依赖任何 Arduino 头文件
 * ═══════════════════════════════════════════════════════════════
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "driver/uart.h"   /* UART_NUM_0/1/2 枚举定义 */

/* ──────────── 日志标签 & 开关 ──────────── */
#define TAG_MAIN   "MAIN"
#define TAG_WIFI   "WIFI"
#define TAG_UDP    "UDP"
#define TAG_BUF    "BUF"

/**
 * DEBUG_ON: 全局调试开关
 *   true  → ESP_LOGI / ESP_LOGW / ESP_LOGE 正常输出
 *   false → 只保留 ESP_LOGE（错误级别）
 *
 * 如果需要关闭全部日志，可在 menuconfig → Component config
 *   → Log output → Default log verbosity 设为 None
 */
#define DEBUG_ON  true

/* ──────────── UART 配置 ──────────── */

/**
 * NUM_UART_DATA: 连接飞控 / 外部设备的 UART
 *   ESP32 有 UART0/1/2，默认 UART0 被 USB 占用（控制台）
 *   所以数据口通常选 UART2（GPIO16 RX, GPIO17 TX）
 */
#define NUM_UART_DATA       UART_NUM_2 //平时为2，测试时为0

/**
 * NUM_UART_DEBUG: 调试输出 UART
 *   默认用 UART0（接 USB-TTL），也是 ESP_LOG 的输出通道
 *   IDF 下 ESP_LOG 已经自动走 stdout → UART0，
 *   所以一般不需要手动操作此 UART
 */
#define NUM_UART_DEBUG      UART_NUM_0

/**
 * 波特率配置：
 *   MAVLink 常用 57600 / 115200 / 921600
 *   对于低速高可靠场景，115200 足够
 */
#define BAUD_DATA           115200
#define BAUD_DEBUG          115200

/**
 * UART 驱动缓冲区大小（字节）
 *   rx_buf: 内核驱动的接收缓冲区，串口慢所以 2048 够用
 *   tx_buf: 内核驱动的发送缓冲区，有 a2wRing 兜底所以可以小
 */
#define UART_DATA_RX_BUF   2048
#define UART_DATA_TX_BUF   1024

/* ──────────── 缓冲区配置 ──────────── */

/**
 * w2aBuf: Wire → Air 线性缓冲区
 *   从 UART 读取数据暂存于此，积累到 flush 条件后一次性通过 UDP 发出
 *   大小 = UDP 最大有效载荷 = 1500(MTU) - 20(IP头) - 8(UDP头) = 1472
 */
#define W2A_BUF_SIZE        1472

/**
 * a2wRing: Air → Wire 环形缓冲区
 *   UDP 接收回调把数据写入此 ring，主循环从中取出写入 UART TX
 *   必须是 2 的幂，便于用位运算代替取模（性能从 100ns → 4ns）
 *   4096 = 约 2.8 个最大 UDP 包的容量，对 MAVLink 低速率绰绰有余
 */
#define A2W_RING_SIZE       32768 //因为用了&运算，必须2次幂。 idf版里udp pbuf小，所以软件buf需要大
#define A2W_RING_MASK       (A2W_RING_SIZE - 1)  /* 位运算掩码，替代 % */

/* ──────────── Wi-Fi 配置 ──────────── */

/** AP 模式默认参数 */
#define AP_SSID             "ESP_UDP_11b"
#define AP_PASS             "11111111"       /* 至少 8 字符 */
#define AP_CHANNEL          13               //通常13最干净 
#define AP_MAX_CONN         4
#define AP_IP_ADDR          "10.0.0.1"
#define AP_IP_GW            "10.0.0.1"
#define AP_IP_NETMASK       "255.255.255.0"

/** STA 模式默认参数（连接家庭路由器） */
#define STA_SSID            ""               /* 为空则跳过 STA，直接开 AP */
#define STA_PASS            "be human"
#define STA_CONNECT_TIMEOUT_MS  10000        /* STA 连接超时 */

/* ──────────── UDP 配置 ──────────── */

/**
 * 端口定义：
 *   ESP32_UDP_PORT: ESP32 自身监听端口
 *   PC_UDP_PORT:    上位机（QGC / MissionPlanner）监听端口
 *   MAVLink 约定通常是 14550(GCS) / 14555(onboard)，
 *   你原先用 5761，这里保持一致
 */
#define ESP32_UDP_PORT      5600
#define PC_UDP_PORT         5700

/**
 * Flush 策略参数（控制 w2aBuf → UDP 发送的节奏）
 *
 *   逻辑：必须同时满足 min 条件，且任一 thres 条件触发即 flush
 *     if (w2aLen >= FLUSH_MIN_SIZE
 *         && elapsed >= FLUSH_MIN_TIME_US
 *         && (w2aLen >= FLUSH_THRES_SIZE || elapsed >= FLUSH_THRES_TIME_US))
 *       → flush!
 *
 *   对 MAVLink：
 *     - 包通常 8~280 字节，心跳 1Hz，指令可达 50Hz
 *     - min_size=1 确保哪怕只有 1 字节也不会永远卡住
 *     - thres_time=1000us (1ms) 保证低延迟
 */
#define FLUSH_MIN_SIZE          1       /* 至少 1 字节才 flush */
#define FLUSH_THRES_SIZE        32      /* 积累 ？B 也触发 flush */
#define FLUSH_MIN_TIME_US       1500     /* 至少间隔 ？us */
#define FLUSH_THRES_TIME_US     3000    /* ？ms 超时强制 flush */
//计算一下， 115200 baud时， 11520 B * 0.003 = 34.56 B
/**
 * 是否接受非锁定 IP 的 UDP 包？
 *   true  = 接收所有来源（广播模式）
 *   false = 只接收第一个发送者（配对模式）
 */
#define LISTEN_BROAD        true

/**
 * 是否向广播地址发送？
 *   true  = 始终广播
 *   false = 锁定目标后点对点发送
 */
#define SEND_BROAD          true

#endif /* APP_CONFIG_H */
