/**
 * ═══════════════════════════════════════════════════════════════
 *  buf_ops.c — 缓冲区操作实现（ESP-IDF 版）
 * ═══════════════════════════════════════════════════════════════
 *
 *  移植自原 Arduino 项目的 buf_rules.cpp
 *  所有 Arduino API 已替换为 IDF uart_driver API
 *
 *  关键改动清单：
 *    Arduino                     →  ESP-IDF
 *    ─────────────────────────────────────────
 *    Serial2.available()         →  uart_get_buffered_data_len()
 *    Serial2.read(buf, len)      →  uart_read_bytes()
 *    Serial2.write(buf, len)     →  uart_write_bytes()
 *    Serial2.availableForWrite() →  uart_get_tx_buffer_free_size()
 *    Serial.begin(baud)          →  uart_driver_install() + uart_param_config()
 *    micros()                    →  esp_timer_get_time()
 * ═══════════════════════════════════════════════════════════════
 */
#include "buf_ops.h"

#include "esp_timer.h"

/* ──────────── 全局缓冲区实例 ──────────── */

uint8_t w2aBuf[W2A_BUF_SIZE];
volatile size_t w2aLen = 0;

uint8_t a2wRing[A2W_RING_SIZE];
volatile size_t a2wHead = 0;
volatile size_t a2wTail = 0;

int64_t lastFlushUs = 0;

/* ══════════════════════════════════════════
 *  ring_to_serial — a2wRing → UART TX
 * ══════════════════════════════════════════
 *
 *  调用时机：主循环每轮调用一次
 *
 *  流程：
 *    1) 查询 UART TX FIFO 空闲字节数
 *    2) 查询 ring 中待发字节数
 *    3) 取 min(空闲, 待发) 作为本次写入量
 *    4) 处理 wrap-around（分两段 memcpy）
 *    5) 更新 tail 指针（原子性由 size_t 对齐保证）
 */
void ring_to_serial(void) {
  size_t tx_free = 0;
  uart_get_tx_buffer_free_size(NUM_UART_DATA, &tx_free);

  size_t used = a2w_used();

  if (tx_free > 0 && used > 0) {
    size_t to_write = tx_free < used ? tx_free : used;

    /* 第一段：tail → 数组末尾 */
    size_t first = A2W_RING_SIZE - a2wTail;
    if (first > to_write) first = to_write;

    uart_write_bytes(NUM_UART_DATA, (const char*)(a2wRing + a2wTail), first);

    /* 第二段：数组开头 → 剩余（如果有 wrap） */
    if (to_write > first) { uart_write_bytes(NUM_UART_DATA, (const char*)a2wRing, to_write - first); }

    /* 更新 tail，位运算取模 */
    a2wTail = (a2wTail + to_write) & A2W_RING_MASK;
  }
}

/* ══════════════════════════════════════════
 *  serial_to_w2a — UART RX → w2aBuf
 * ══════════════════════════════════════════
 *
 *  调用时机：主循环每轮调用一次
 *
 *  流程：
 *    1) 查询内核 UART RX 缓冲区已有字节数
 *    2) 计算 w2aBuf 剩余空间 = max_payload - w2aLen
 *    3) 读取 min(剩余, 已有) 字节到 w2aBuf + w2aLen
 *    4) 更新 w2aLen
 *
 *  注意：uart_read_bytes 第 4 个参数是等待 tick 数，
 *        传 0 = 不等待，立即返回（非阻塞）
 */
void serial_to_w2a(int32_t max_payload) {
  size_t buffered = 0;
  uart_get_buffered_data_len(NUM_UART_DATA, &buffered);

  if (buffered > 0) {
    int32_t can_read = max_payload - (int32_t)w2aLen;
    if (can_read <= 0) return;

    int32_t actual = can_read < (int32_t)buffered ? can_read : (int32_t)buffered;

    int len = uart_read_bytes(NUM_UART_DATA, w2aBuf + w2aLen, (uint32_t)actual, 0); /* 0 = 不阻塞 */
    if (len > 0) { w2aLen += len; }
  }
}

/* ══════════════════════════════════════════
 *  uart_data_init — 初始化数据 UART 驱动
 * ══════════════════════════════════════════
 *
 *  替代 Arduino 的 Serial2.begin(115200)
 *
 *  IDF UART 初始化三步：
 *    1) uart_param_config   — 配置波特率、数据位等电气参数
 *    2) uart_set_pin        — 绑定 GPIO（如果用非默认引脚）
 *    3) uart_driver_install — 安装中断驱动，分配 RX/TX 缓冲区
 *
 *  缓冲区关系：
 *    UART 硬件有 128B FIFO（固定，无法改）
 *    + uart_driver_install 创建的软件缓冲区（我们设 2048/1024）
 *    + 我们的 a2wRing / w2aBuf（应用层缓冲区）
 *    三级缓冲，层层吸收突发
 */
void uart_data_init(void) {
  // 1️⃣
  uart_config_t uart_config = {
      .baud_rate = BAUD_DATA,  // 波特率

      .data_bits = UART_DATA_8_BITS,  // 数据位：每帧数据位数
                                      // 几乎永远是 8，不用改
      .parity = UART_PARITY_DISABLE,  // 校验位：奇偶校验
                                      // MAVLink 自带 CRC，不需要 UART 校验

      .stop_bits = UART_STOP_BITS_1,  // 停止位：1 或 2
                                      // 1 是标准，2 用于慢速设备兼容

      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,  // 硬件流控：RTS/CTS
                                              // 飞控通常不接这两根线，关掉

      .source_clk = UART_SCLK_APB,  // 时钟源：APB 总线时钟（80MHz）
                                    // 另一个选项 REF_TICK(1MHz)用于低功耗
                                    // 你用 80MHz flash，APB 也是 80MHz，没问题
  };
  // ▼▼▼ 关键：必须设置引脚 ▼▼▼
  // 参数：UART号, TX引脚, RX引脚, RTS引脚(-1=不用), CTS引脚(-1=不用)
  // 2️⃣
  uart_set_pin(NUM_UART_DATA, 17, 16, -1, -1);
  // 3️⃣
  ESP_ERROR_CHECK(uart_driver_install(NUM_UART_DATA, UART_DATA_RX_BUF, UART_DATA_TX_BUF, 0, NULL, 0));
  /*
   * uart_driver_install 参数说明：
   *   参数 1: UART 编号
   *   参数 2: RX 软件缓冲区大小（必须 > 128，即 > FIFO 大小）
   *   参数 3: TX 软件缓冲区大小（0 = 不用软件缓冲，直接写 FIFO）
   *   参数 4: 事件队列大小（0 = 不使用事件通知）
   *   参数 5: 事件队列句柄指针
   *   参数 6: 中断分配标志
   */
#ifdef _TIPS_
  uart_driver_install(UART_NUM, BUF_SIZE, BUF_SIZE,

                      // ↓ 第 4 个参数：事件队列深度
                      0,  // 0 = 不创建事件队列
                          // >0 = 创建队列，可以收 UART_DATA/UART_BREAK/
                          //      UART_BUFFER_FULL 等事件通知
                          // 你现在轮询读，不需要事件，填 0

                      // ↓ 第 5 个参数：事件队列句柄
                      NULL,  // 配合参数 4，不用队列就填 NULL
                             // 如果参数 4 > 0，这里传 QueueHandle_t* 接收句柄

                      // ↓ 第 6 个参数：中断分配标志
                      0  // 0 = 默认标志（中断分配到当前核心）
                         // ESP_INTR_FLAG_IRAM = 中断处理函数放 IRAM
                         //   （flash cache miss 时仍能响应，高实时性场景用）
                         // ESP_INTR_FLAG_LEVEL3 = 更高中断优先级
                         // 你现在不需要特殊设置，填 0 即可
  );
#endif
  // 4️⃣
  ESP_ERROR_CHECK(uart_param_config(NUM_UART_DATA, &uart_config));



  ESP_LOGI(TAG_BUF, "UART%d 初始化完成 (波特率 %d)", NUM_UART_DATA, BAUD_DATA);
}
