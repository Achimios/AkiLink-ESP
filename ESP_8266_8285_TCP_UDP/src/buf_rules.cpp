#include "buf_rules.h"

// ============================================================
//  ESP8266 / ESP8285  buf_rules.cpp
//  从 ESP32 移植: uart_* IDF API → Arduino Serial API
// ============================================================

// W2A 线性缓冲区
int32_t MODE_PAYLOAD_MAX = w2aSize;
uint8_t w2aBuf[w2aSize];
volatile size_t w2aLen = 0;

// A2W 环形缓冲区
uint8_t  a2wRing[A2W_RING_SIZE];
volatile size_t a2wHead = 0;
volatile size_t a2wTail = 0;
int16_t  packetSize = 0;
size_t   a2wToWrite = A2W_RING_SIZE_SUB_ONE;       // 初始整个缓冲区空
volatile int16_t ringLack = -(int16_t)a2wToWrite;   // 负数=剩余空间

// Flush
unsigned long lastFlushUs = 0;


// ----------------------------------------------------------
//  Air → Wire: 环形缓冲区 → 串口 TX FIFO (非阻塞)
// ----------------------------------------------------------
//  ESP32 用 uart_get_tx_buffer_free_size + uart_write_bytes
//  ESP8266 用 Serial.availableForWrite() + Serial.write()
//
//  ESP8266 Serial TX 没有软件缓冲区，只有128B硬件FIFO
//  所以 availableForWrite() 返回 FIFO 剩余空间 (0~128)
//  每次 loop 推一点，完全非阻塞
// ----------------------------------------------------------
void RingToSerial() {
    size_t tx_free = S_DATA.availableForWrite();  // 硬件FIFO空位 (max 128)
    size_t ringUsed = a2wUsed();

    if (tx_free > 0 && ringUsed > 0) {
        size_t to_wri = min(tx_free, ringUsed);

        // 第一段: Tail → 数组末尾
        size_t firstPart = min(to_wri, A2W_RING_SIZE - a2wTail);
        S_DATA.write(a2wRing + a2wTail, firstPart);

        // 第二段: 回卷到数组开头
        if (to_wri > firstPart) {
            S_DATA.write(a2wRing, to_wri - firstPart);
        }

        a2wTail = (a2wTail + to_wri) & A2W_RING_SIZE_SUB_ONE;
    }
}


// ----------------------------------------------------------
//  Wire → Air: 串口 RX → w2aBuf 线性缓冲区 (非阻塞)
// ----------------------------------------------------------
//  ESP32 用 uart_get_buffered_data_len + uart_read_bytes
//  ESP8266 用 Serial.available() + Serial.readBytes()
//  已在 init 中设置 setTimeout(0)，readBytes 不会阻塞
// ----------------------------------------------------------
void serialToW2aBuf() {
    size_t buffered = S_DATA.available();    // RX 软件缓冲区中已有的字节数

    if (buffered > 0) {
        int16_t max_can_read = MODE_PAYLOAD_MAX - w2aLen;
        int16_t actual_read  = min(max_can_read, (int16_t)buffered);

        if (actual_read > 0) {
            // readBytes 直接读到 w2aBuf 偏移处，一次拷贝
            size_t len = S_DATA.readBytes((char*)(w2aBuf + w2aLen), actual_read);
            if (len > 0) { w2aLen += len; }
        }
    }
}


// ----------------------------------------------------------
//  初始化数据串口 Serial (UART0: TX=GPIO1, RX=GPIO3)
// ----------------------------------------------------------
void init_s_data() {
    S_DATA.setRxBufferSize(s_dat_rx_sz);   // 必须在 begin() 之前调用!
    S_DATA.setTimeout(0);                  // readBytes 不阻塞等待
    S_DATA.begin(BAUD_DATA);

    // ESP8266 UART0 引脚固定 (TX=GPIO1, RX=GPIO3)，无需 uart_set_pin
    // 硬件FIFO: RX 128B + 软件RX s_dat_rx_sz
    // 硬件FIFO: TX 128B (无软件TX缓冲区)
}


// ----------------------------------------------------------
//  初始化调试串口 Serial1 (UART1: TX=GPIO2, 仅TX无RX)
//  ⚠️ 条件性初始化: 不初始化 → GPIO2 留给 LED
// ----------------------------------------------------------
void init_s_debug() {
#ifdef _FACTORY_DEBUG
    // 编译期工厂调试: 无条件启用 Serial1
    S_DEBUG.begin(FCT_BAUD_DBG);
    return;
#else
    // 运行时: 仅 DebugON 时才启用 Serial1
    if (DEBUG_ON) {
        S_DEBUG.begin(BAUD_DEBUG);
    }
    // DebugON=false → 不调 begin() → GPIO2 保持为普通GPIO → LED可用
#endif
}
