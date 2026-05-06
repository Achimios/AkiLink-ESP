/**
 * ═══════════════════════════════════════════════════════════════
 *  wifi_setup.h — Wi-Fi 初始化与管理（ESP-IDF 版）
 * ═══════════════════════════════════════════════════════════════
 *
 *  移植自原 Arduino 项目的 wifi_config.h / wifi_config.cpp
 *
 *  与 Arduino 版本的核心差异：
 *  ┌────────────────────┬──────────────────────────────────────┐
 *  │ Arduino            │ ESP-IDF                              │
 *  ├────────────────────┼──────────────────────────────────────┤
 *  │ WiFi.begin()       │ esp_wifi_init + esp_wifi_start       │
 *  │ WiFi.status()      │ 事件系统 WIFI_EVENT / IP_EVENT       │
 *  │ WiFi.localIP()     │ esp_netif_get_ip_info()              │
 *  │ WiFi.softAP()      │ esp_wifi_set_config(WIFI_IF_AP, ...) │
 *  │ WiFi.softAPConfig  │ esp_netif_set_ip_info()              │
 *  │ WiFi.setPS()       │ esp_wifi_set_ps(WIFI_PS_NONE)        │
 *  │ WiFi.setProtocol() │ esp_wifi_set_protocol(interface, ..) │
 *  └────────────────────┴──────────────────────────────────────┘
 *
 *  802.11b 强制说明：
 *    通过 esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B)
 *    强制 11b DSSS 模式。11b 1Mbps 的调制就是 DBPSK，
 *    这是 ESP32 能达到的最高接收灵敏度（约 -98dBm）。
 *    Long GI 概念不适用于 11b（GI 是 OFDM 的参数）。
 * ═══════════════════════════════════════════════════════════════
 */
#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

#include "app_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "lwip/ip_addr.h"   /* ip_addr_t for wifi_get_broadcast_addr() */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Wi-Fi 连接状态（全局标志）
 *
 * 由事件回调异步更新，主循环读取。
 * 对应原 Arduino 代码中的 deviceConnected 变量。
 */
extern volatile bool wifi_connected;

/**
 * 是否处于 AP 模式？
 *
 * true  = ESP32 自己是热点（AP）
 * false = ESP32 连接到路由器（STA）
 */
extern bool is_ap_mode;

/**
 * 初始化 Wi-Fi 子系统
 *
 * 完整流程：
 *   1) NVS Flash 初始化（Wi-Fi 校准数据存储在 NVS 中）
 *   2) TCP/IP 协议栈初始化（esp_netif）
 *   3) 创建默认事件循环
 *   4) 尝试 STA 连接（如果配置了 STA_SSID）
 *   5) STA 失败 → 自动切 AP 模式
 *   6) 关闭省电、强制 11b、固定信道、最大功率
 *
 * 此函数会阻塞到获取 IP 或 AP 启动完成。
 */
void wifi_init(void);

/**
 * 检查 Wi-Fi 连接状态
 *
 * 在主循环中周期调用，更新 wifi_connected 标志。
 * AP 模式下检查是否有 STA 连入；STA 模式下检查连接。
 */
void wifi_check_connection(void);

/**
 * 获取广播地址
 *
 * AP 模式：固定返回 10.0.0.255
 * STA 模式：根据当前 IP 和子网掩码计算
 *
 * @param brd_addr [输出] 计算得到的广播 ip_addr_t
 */
void wifi_get_broadcast_addr(ip_addr_t *brd_addr);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_SETUP_H */
