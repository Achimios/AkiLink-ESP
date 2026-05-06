/**
 * ═══════════════════════════════════════════════════════════════
 *  main — ESP-IDF 入口（MAVLink UDP 透传）
 * ═══════════════════════════════════════════════════════════════
 *
 *  IDF 和 Arduino 入口差异：
 *    Arduino:  setup() 跑一次 + loop() 无限循环
 *    IDF:      app_main() 就是你的"裸任务"，用 while(1) 自己循环
 *
 *  整体架构：
 *    app_main()
 *      ├── uart_data_init()     串口驱动初始化
 *      ├── wifi_init()          Wi-Fi 初始化（含 NVS / 事件 / AP or STA）
 *      ├── air_udp_init()       UDP PCB 创建 + 绑定 + 注册回调
 *      └── while(1)             主循环
 *            └── air_udp_update()  三向数据搬运
 *
 *  数据流全景：
 *
 *    飞控 ←→ UART ←→ [w2aBuf / a2wRing] ←→ UDP ←→ 上位机(QGC)
 *              ↑                                      ↑
 *         serial_to_w2a()                      udp_recv_cb()
 *         ring_to_serial()                     udp_sendto()
 * ═══════════════════════════════════════════════════════════════
 */

#include "app_config.h"
#include "buf_ops.h"
#include "wifi_setup.h"
#include "air_udp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * app_main — IDF 入口函数
 *
 * 注意：
 *   - 这个函数运行在 main task 中（默认栈 3584 字节）
 *   - 如果你需要更大的栈，在 menuconfig → Component config
 *     → ESP System Settings → Main task stack size 中调大
 *   - 如果需要多核并行，可以用 xTaskCreatePinnedToCore
 *     把 UDP 更新和串口读写拆到不同核上
 */
void app_main(void)
{
    // esp_log_set_vprintf(custom_vprintf);  // custom_vprintf类型为 int (*)(const char *, va_list)，需要自己实现一个函数来重定向日志输出到 UART，没必要
    ESP_LOGI(TAG_MAIN, "═══════════════════════════════════════");
    ESP_LOGI(TAG_MAIN, "  MAVLink UDP 透传 (ESP-IDF + 11b DSSS)");
    ESP_LOGI(TAG_MAIN, "═══════════════════════════════════════");

    /* ── 1. UART 初始化 ──
     *
     * 必须在 Wi-Fi 之前初始化，
     * 因为飞控可能一上电就在发数据（MAVLink 心跳），
     * 我们需要尽早开始接收。
     */
    uart_data_init();

    /* ── 2. Wi-Fi 初始化 ──
     *
     * 内部流程：NVS → netif → 事件循环 → 尝试 STA → 回退 AP
     * 阻塞到获取 IP 或 AP 启动完成。
     * 同时会：强制 11b 协议、关闭省电、最大功率
     */
    wifi_init();

    /* ── 3. UDP 初始化 ──
     *
     * 创建 RAW lwIP UDP PCB，绑定端口，注册接收回调。
     * 从此刻起，任何发到 ESP32_UDP_PORT 的 UDP 包
     * 都会通过 udp_recv_cb() 自动写入 a2wRing。
     */
    air_udp_init();

    ESP_LOGI(TAG_MAIN, "系统就绪，进入主循环");

    /* ── 4. 主循环 ──
     *
     * 每轮做三件事（全部在 air_udp_update 内部）：
     *   ① a2wRing → UART TX  （收到的 UDP 包 → 飞控）
     *   ② UART RX → w2aBuf   （飞控数据 → 线性缓冲区）
     *   ③ flush → udp_sendto （缓冲区数据 → 上位机）
     *
     * vTaskDelay(1) = 让出 1 个 RTOS tick（默认 10ms）
     * 给 Wi-Fi 驱动和其他系统任务执行的机会。
     *
     * 如果你需要更低延迟，可以把 tick rate 从 100Hz 调到 1000Hz
     * （menuconfig → Component config → FreeRTOS → Tick rate）
     * 这样 vTaskDelay(1) = 1ms 而不是 10ms。
     */
    while (1) {
        air_udp_update();
        vTaskDelay(pdMS_TO_TICKS(1));  /* 1ms 间隔，足够 MAVLink 实时性 */
    }
}