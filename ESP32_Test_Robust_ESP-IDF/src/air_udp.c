/**
 * ═══════════════════════════════════════════════════════════════
 *  air_udp.c — RAW lwIP UDP 收发实现（ESP-IDF 版）
 * ═══════════════════════════════════════════════════════════════
 *
 *  移植自原 Arduino 项目的 air_udp.cpp
 *
 *  改动清单（Arduino → IDF）：
 *  ┌──────────────────────────┬────────────────────────────────┐
 *  │ 原 Arduino               │ IDF 替换                       │
 *  ├──────────────────────────┼────────────────────────────────┤
 *  │ WiFi.localIP()           │ wifi_get_broadcast_addr()      │
 *  │ micros()                 │ esp_timer_get_time()           │
 *  │ delay(ms)                │ vTaskDelay(pdMS_TO_TICKS(ms))  │
 *  │ DEBUG_PRINTLN(String(x)) │ ESP_LOGI(TAG, fmt, ...)        │
 *  │ checkWifiConnection()    │ wifi_check_connection()        │
 *  │ serialToW2aBuf()         │ serial_to_w2a(max_payload)     │
 *  │ RingToSerial()           │ ring_to_serial()               │
 *  │ shouldFlush()            │ should_flush()                 │
 *  └──────────────────────────┴────────────────────────────────┘
 *
 *  核心逻辑完全保留，只是 API "外壳" 换了。
 *
 *  数据流示意：
 *
 *  【发送方向：Wire → Air】
 *    飞控 UART TX → ESP32 UART RX
 *      → uart_read_bytes() → w2aBuf（线性缓冲区）
 *      → should_flush() 条件满足
 *      → pbuf_alloc(PBUF_RAM) + memcpy w2aBuf → pbuf
 *      → udp_sendto(pcb, pbuf, dest_ip, dest_port)
 *      → pbuf_free()
 *      → w2aLen = 0（复位）
 *
 *  【接收方向：Air → Wire】
 *    UDP 包到达 → lwIP 内核调用 udp_recv_cb()
 *      → 遍历 pbuf 链表（UDP 包可能被拆为多段 pbuf）
 *      → memcpy pbuf→payload → a2wRing（环形缓冲区）
 *      → pbuf_free()
 *    主循环调用 ring_to_serial()
 *      → a2wRing → uart_write_bytes() → 飞控 UART RX
 * ═══════════════════════════════════════════════════════════════
 */
#include "air_udp.h"
#include "buf_ops.h"
#include "wifi_setup.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

/* ══════════ 全局 & 模块级变量 ══════════ */

/** UDP 控制块 */
struct udp_pcb *air_udp_pcb = NULL;

/**
 * 目标锁定机制：
 *  第一个给 ESP32 发 UDP 包的人，
 *  他的 IP 和 Port 会被记住，之后只接受他的数据。
 *  这就是你原来代码中的 "配对" 逻辑。
 */
static ip_addr_t  s_target_ip;
static uint16_t   s_target_port = PC_UDP_PORT;
static bool       s_target_locked = false;

/** 广播地址（AP 模式 = 10.0.0.255，STA 模式 = 动态计算） */
static ip_addr_t  s_broadcast_ip;

/* ══════════════════════════════════════════
 *  udp_recv_cb — UDP 接收回调 (Air → Ring)
 * ══════════════════════════════════════════
 *
 *  这个函数在 lwIP 的 tcpip_thread 中被调用！
 *  不能做任何阻塞操作（不能 printf、不能 vTaskDelay、
 *  不能 acquire mutex with timeout）。
 *
 *  参数说明：
 *    arg  — udp_recv() 注册时传入的 user data（我们传 NULL）
 *    pcb  — 触发回调的 UDP 控制块
 *    p    — 接收到的数据（pbuf 链表）
 *    addr — 发送者的 IP 地址
 *    port — 发送者的端口号
 *
 *  pbuf 链表：
 *    为什么是链表？因为 lwIP 为了减少大块内存分配，
 *    会把一个 UDP 包拆成多个小 pbuf 串起来。
 *    你需要遍历 p→next 来处理完整数据。
 *    这和 Arduino WiFiUdp.h 不同——WiFiUdp 会在堆上
 *    new 一个大缓冲区存整个包，我们这里更高效。
 *
 *  重要：回调结束前必须调用 pbuf_free(p) 释放内存！
 */
static void udp_recv_cb(void *arg,
                         struct udp_pcb *pcb,
                         struct pbuf *p,
                         const ip_addr_t *addr,
                         u16_t port)
{
    if (p == NULL) return;

    /* ── 目标锁定逻辑 ── */
    if (!s_target_locked) {
        /* 第一个发送者成为"配对目标" */
        s_target_ip   = *addr;
        s_target_port = port;
        s_target_locked = true;
        /* 注意：回调中不能用 ESP_LOGI（它可能阻塞）。
         * 如果需要调试，可以用 ets_printf（非阻塞打印）。 */
    } else {
        /* 非配对 IP → 丢弃（除非开了广播监听） */
        if (!ip_addr_cmp(&s_target_ip, addr) && !LISTEN_BROAD) {
            pbuf_free(p);
            return;
        }
    }

    //*1️1️1️1️== airRX → a2w环形Buf    ==1️1️1️1️*
    struct pbuf *q = p;
    while (q != NULL) {
        int16_t pkt_len = (int16_t)q->len;
        size_t  to_write = 0;
        int16_t first_part = check_ring_space(pkt_len, &to_write);

        if (first_part >= 0) {
            /* 第一段拷贝：head → 数组末尾 */
            memcpy(a2wRing + a2wHead, q->payload, first_part);
            /* 第二段拷贝：数组开头 → 剩余（如果有 wrap-around） */
            if (to_write > (size_t)first_part) {
                memcpy(a2wRing,
                       (uint8_t *)q->payload + first_part,
                       to_write - first_part);
            }
            /* 更新 head（原子性由 32 位对齐保证） */
            a2wHead = (a2wHead + to_write) & A2W_RING_MASK;
        } else {
            /* 环形缓冲区满了，丢弃后续数据 */
            break;
        }

        q = q->next;  /* 遍历 pbuf 链表的下一段 */
    }

    /* ── 释放 pbuf（必须！否则内存泄漏） ── */
    pbuf_free(p);
}

/* ══════════════════════════════════════════
 *  air_udp_init — 初始化 UDP 模块
 * ══════════════════════════════════════════
 *
 *  对应原 Arduino 的 airUdp_begin()
 *
 *  调用时机：wifi_init() 之后
 *
 *  RAW lwIP UDP 初始化三步：
 *    1) udp_new()  — 分配一个 udp_pcb 结构体
 *    2) udp_bind() — 绑定本地 IP（ANY）和端口
 *    3) udp_recv() — 注册接收回调函数
 */
void air_udp_init(void)
{
    ESP_LOGI(TAG_UDP, "初始化 UDP (RAW lwIP 模式)...");

    /* 计算广播地址（依赖 Wi-Fi 模式） */
    wifi_get_broadcast_addr(&s_broadcast_ip);

    /* 创建 UDP PCB */
    air_udp_pcb = udp_new();
    if (air_udp_pcb == NULL) {
        ESP_LOGE(TAG_UDP, "udp_new() 失败！内存不足");
        return;
    }

    /* 绑定到所有本地接口 + 指定端口 */
    err_t err = udp_bind(air_udp_pcb, IP_ADDR_ANY, ESP32_UDP_PORT);
    if (err != ERR_OK) {
        ESP_LOGE(TAG_UDP, "udp_bind() 失败，错误码: %d", err);
        udp_remove(air_udp_pcb);
        air_udp_pcb = NULL;
        return;
    }

    /**
     * 注册接收回调
     *
     * 从此刻起，任何发到 ESP32_UDP_PORT 的 UDP 包
     * 都会触发 udp_recv_cb()
     *
     * 第三个参数 NULL 是 user data，会传给回调的 arg 参数。
     * 我们不需要，所以传 NULL。
     */
    udp_recv(air_udp_pcb, udp_recv_cb, NULL);

    ESP_LOGI(TAG_UDP, "✅ UDP 监听端口: %d", ESP32_UDP_PORT);
}

/* ══════════════════════════════════════════
 *  air_udp_update — UDP 主循环
 * ══════════════════════════════════════════
 *
 *  对应原 Arduino 的 airUdp_update()
 *
 *  每轮做三件事：
 *    ① a2wRing → UART TX（收到的 UDP 数据 → 飞控串口）
 *    ② UART RX → w2aBuf（飞控串口数据 → 线性缓冲区）
 *    ③ 检查 flush → UDP 发送（线性缓冲区 → 空中）
 */
void air_udp_update(void)
{
    /* ── 检查 Wi-Fi 连接状态 ── */
    wifi_check_connection();

    //*2️2️2️2️== a2w环形Buf → SerialTX ==2️2️2️2️*
    ring_to_serial();

    /* 只有 Wi-Fi 已连接 且 UDP PCB 已创建 才进行收发 */
    if (!wifi_connected || air_udp_pcb == NULL) {
        return;
    }

    //*3️3️3️3️== SerialRX → w2a线性Buf ==3️3️3️3️*
    serial_to_w2a(W2A_BUF_SIZE);   /* max_payload = UDP 最大载荷 */

     //*4️4️4️4️== w2a线性Buf → AirTX    ==4️4️4️4️*
    if (should_flush()) {
        /**
         * pbuf 分配策略：PBUF_RAM
         *
         * PBUF_TRANSPORT: 在 payload 前预留 UDP/IP 头的空间
         * PBUF_RAM:       分配连续内存，拷贝数据进去
         *
         * 为什么用 PBUF_RAM 而不是 PBUF_REF？
         *   PBUF_REF 只存指针（指向 w2aBuf），不拷贝数据。
         *   看似更快，但 udp_sendto() 之后 lwIP 可能还在
         *   异步发送中访问 w2aBuf，而此时你已经在主循环中
         *   往 w2aBuf 写新数据了 → 数据竞争！
         *   所以必须用 PBUF_RAM 拷贝一份。
         *   拷贝 1000 字节约 2~3 微秒，完全可以接受。
         */
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, w2aLen, PBUF_RAM);
        if (p == NULL) {
            ESP_LOGW(TAG_UDP, "pbuf_alloc 失败");
            return;
        }

        /* 拷贝数据到 pbuf */
        memcpy(p->payload, w2aBuf, w2aLen);

        /**
         * 目标地址决策（和你原来的逻辑一致）：
         *   - 如果已锁定目标 && 非广播模式 → 点对点发送
         *   - 否则 → 广播发送
         */
        const ip_addr_t *dest_ip;

        if (s_target_locked && !SEND_BROAD) {
            dest_ip   = &s_target_ip;
        } else {
            dest_ip   = &s_broadcast_ip;
        }

        /* 发送！ */
        err_t err = udp_sendto(air_udp_pcb, p, dest_ip, s_target_port);

        /* 释放 pbuf（发送完毕后 lwIP 已经把数据交给驱动了） */
        pbuf_free(p);

        if (err == ERR_OK) {
            /* 发送成功：清空缓冲区、更新时间戳 */
            w2aLen = 0;
            lastFlushUs = esp_timer_get_time();
        } else {
            /**
             * 发送失败处理：
             *   保留 w2aLen 不清零 → 下一轮会重试。
             *   常见失败原因：
             *     ERR_MEM (-1):  WiFi TX buffer 满了
             *     ERR_RTE (-4):  路由表找不到目标
             *   短暂 delay 释放 CPU，让 WiFi 驱动有时间排空队列
             */
            ESP_LOGW(TAG_UDP, "udp_sendto 失败, err=%d", err);
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}
