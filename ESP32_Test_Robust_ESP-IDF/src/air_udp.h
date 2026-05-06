/**
 * ═══════════════════════════════════════════════════════════════
 *  air_udp.h — RAW lwIP UDP 收发模块（ESP-IDF 版）
 * ═══════════════════════════════════════════════════════════════
 *
 *  移植自原 Arduino 项目的 air_udp.h
 *
 *  为什么用 RAW lwIP 而不是 BSD Socket？
 *    你原来的代码就是 RAW lwIP（udp_pcb + 回调），
 *    在 IDF 下可以几乎原样使用，因为 IDF 底层就是 lwIP。
 *    RAW 路径的好处：
 *      - 回调中直接拿到 pbuf，可以零拷贝写入 ring buffer
 *      - 无需创建额外的接收线程
 *      - 延迟更低（少了 socket 层的队列和唤醒开销）
 *    代价：
 *      - 回调在 lwIP tcpip_thread 中执行，不能做阻塞操作
 *      - pbuf 生命周期管理需要自己负责（回调结束前必须 free）
 * ═══════════════════════════════════════════════════════════════
 */
#ifndef AIR_UDP_H
#define AIR_UDP_H

#include "app_config.h"
#include "lwip/udp.h"     /* RAW lwIP UDP API: udp_new, udp_bind, udp_recv, udp_sendto */
#include "lwip/ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * UDP 控制块指针（全局可见）
 *
 * udp_pcb 是 lwIP 对一个 UDP "连接" 的内部表示，
 * 类似于 BSD socket 的 fd，但更底层。
 * 通过它调用 udp_sendto 发包、udp_recv 注册回调。
 */
extern struct udp_pcb *air_udp_pcb;

/**
 * 初始化 UDP
 *
 * 调用时机：wifi_init() 之后、主循环之前
 *
 * 做的事：
 *   1) 计算广播地址（AP 和 STA 不同）
 *   2) udp_new()       — 创建 PCB
 *   3) udp_bind()      — 绑定本地端口
 *   4) udp_recv()      — 注册接收回调
 */
void air_udp_init(void);

/**
 * UDP 主循环更新
 *
 * 调用时机：主循环每轮调用
 *
 * 做的事：
 *   1) a2wRing → UART TX（ring_to_serial）
 *   2) UART RX → w2aBuf（serial_to_w2a）
 *   3) 检查 flush 条件 → udp_sendto
 *
 * 接收方向（Air → Wire）已在 udp_recv_cb 回调中自动完成。
 */
void air_udp_update(void);

#ifdef __cplusplus
}
#endif

#endif /* AIR_UDP_H */
