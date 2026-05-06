#include "buf_rules.h"

int32_t MODE_PAYLOAD_MAX = w2aSize;
// 🧑‍💻 缓冲区 🧑‍💻   🧑‍💻   🧑‍💻   🧑‍💻
uint8_t w2aBuf[w2aSize];
volatile size_t w2aLen = 0;

// uint8_t a2wBuf[a2wSize];
// volatile size_t a2wLen = 0;

// uint8_t tempBuf[tempBufSize];
uint8_t a2wRing[A2W_RING_SIZE];
volatile size_t a2wHead = 0;  // 写入位置（AIR 写入）
volatile size_t a2wTail = 0;  // 读取位置（Serial 读取）
int16_t packetSize = 0;
size_t a2wToWrite = A2W_RING_SIZE_SUB_ONE;  // 初始时整个缓冲区都是空的
volatile int16_t ringLack = -a2wToWrite;    // 初始时缺少的字节数为负数，表示剩余空间
int16_t JUNK_SIZE = junk_size;
unsigned long lastFlushUs = 0;

/**
 * @brief 极限优化：Air -> Wire (a2wRing -> SerialTX)
 * 零阻塞写入硬件发送队列
 */
void RingToSerial() {
  size_t tx_free_fifo;  // 1. 获取硬件发送缓冲区当前可用空间 (替代 S_DATA->availableForWrite)
  uart_get_tx_buffer_free_size(NUM_S_DATA, &tx_free_fifo);

  size_t ringUsed = a2wUsed();  // 2. 获取环形缓冲区中待发的数据量

  if (tx_free_fifo > 0 && ringUsed > 0) {
    size_t to_wri = min(tx_free_fifo, ringUsed);  // 这次能发多少：取 硬件空位 和 现有数据 的最小值

    size_t firstPart = min(to_wri, A2W_RING_SIZE - a2wTail);  // 计算第一段（Tail 到 数组末尾）的长度

    uart_write_bytes(NUM_S_DATA, a2wRing + a2wTail, firstPart);  // 1️⃣ 第一写：直接推送到底层驱动

    if (to_wri > firstPart)
      uart_write_bytes(NUM_S_DATA, a2wRing, to_wri - firstPart);  // 2️⃣ 第二写：如果有回卷，发送开头数据

    a2wTail = (a2wTail + to_wri) & A2W_RING_SIZE_SUB_ONE;  // 3️⃣ 更新 Tail (位运算优化)
  }
}

/**
 * @brief 极限优化：Wire -> Air (SerialRX -> w2aBuf)
 * 直接从硬件驱动缓冲区拉取数据，跳过 Arduino 中间层
 */
void serialToW2aBuf() {
  size_t buffered_size;
  uart_get_buffered_data_len(NUM_S_DATA, &buffered_size);  // 查询驱动缓冲区里现在攒了多少字节

  if (buffered_size > 0) {
    int16_t max_can_read = MODE_PAYLOAD_MAX - w2aLen;  // 计算 w2aBuf 还能吃下多少
    int16_t actual_read = min(max_can_read, (int16_t)buffered_size);

    if (actual_read > 0) {
      // 核心优化：直接读取到 w2aBuf 的偏移地址处  // 这是从内核缓冲区到应用缓冲区的唯一一次拷贝
      int len = uart_read_bytes(NUM_S_DATA, w2aBuf + w2aLen, actual_read,
                                0);  // 最后一个参数是等待时间，0表示不等待，立即返回
      if (len > 0) { w2aLen += len; }
    }
  }
}

/**
 * @brief 初始化底层 UART 驱动
 * 取代原来的 Serial.begin()
 */


void init_s_debug_raw() {
  uart_config_t uart_config = {
      .baud_rate = (int)BAUD_DEBUG,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_APB,
  };
  //   uart_set_pin(NUM_S_DEBUG, 1, 3, -1, -1);//对于uart0自动的，没必要手动。要改再设置
  uart_driver_install(NUM_S_DEBUG, s_dbg_rx_sz, s_dbg_tx_sz, 0, NULL,
                      0);  // 调试rx其实压根不需要buf，但是为了成功初始化，还是给个128吧
  uart_param_config(NUM_S_DEBUG, &uart_config);
}


void init_s_data_raw() {
  uart_config_t uart_config = {
      .baud_rate = (int)BAUD_DATA,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_APB,
  };

  // 参数：UART号, TX引脚, RX引脚, RTS引脚(-1=不用), CTS引脚(-1=不用)
  if (NUM_S_DEBUG != NUM_S_DATA) uart_set_pin(NUM_S_DATA, 17, 16, -1, -1);  // uart2必须手动配置引脚，而uart0不用
  // uart_set_pin(uart_num, tx_io, rx_io, rts_io, cts_io)

  // 安装驱动：RX/TX 缓冲区设为 1024，足够应对突发大数据
  uart_driver_install(NUM_S_DATA, s_dat_rx_sz, s_dat_tx_sz, 0, NULL, 0);  // 已经有ring了，数据tx其实不太需要
  
  // uart_driver_install(uart_num, rx_buffer_size, tx_buffer_size, queue_size, uart_queue, intr_alloc_flags)
  uart_param_config(NUM_S_DATA, &uart_config);
}
