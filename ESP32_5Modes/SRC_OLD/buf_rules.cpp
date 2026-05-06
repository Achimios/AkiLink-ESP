#include "buf_rules.h"


int32_t MODE_PAYLOAD_MAX = w2aSize;
// 🧑‍💻 缓冲区 🧑‍💻   🧑‍💻   🧑‍💻   🧑‍💻
uint8_t w2aBuf[w2aSize];
volatile size_t w2aLen = 0;

uint8_t a2wBuf[a2wSize];
// volatile size_t a2wLen = 0;

// uint8_t tempBuf[tempBufSize];
uint8_t a2wRing[A2W_RING_SIZE];
volatile size_t a2wHead = 0;  // 写入位置（AIR 写入）
volatile size_t a2wTail = 0;  // 读取位置（Serial 读取）
int16_t packetSize = 0;
size_t a2wToWrite = A2W_RING_SIZE_SUB_ONE; //初始时整个缓冲区都是空的
volatile int16_t ringLack = -a2wToWrite; //初始时缺少的字节数为负数，表示剩余空间

unsigned long lastFlushUs = 0;


// ========== A2W环形缓冲区定义 ==========

// 写入环形缓冲区（从 air 读取后调用）
// size_t readAir_writeToRing(const uint8_t* data, size_t len) {
//     size_t written = 0;
//     while (written < len && a2wFree() > 0) {
//         a2wRing[a2wHead] = data[written++];
//         a2wHead = (a2wHead + 1) % A2W_RING_SIZE;
//     }
//     return written;
// }

// ⚡性能优化提示⚡
// RAM 到 RAM（memcpy）	~1-2 GB/s	CPU 直接操作内存
// RAM 到 FIFO（Serial.write）	~100 MB/s	写入硬件寄存器，稍慢
// FIFO 发送到线路	~14 KB/s @ 115200	波特率限制
// RAM到RAM快，所以while就行，如果有强迫症，就用下面的批量写入

// size_t readAir_writeToRing(const uint8_t* data, size_t len) {
//     size_t free = a2wFree();
//     if (free == 0) return 0;
//     size_t to_wri = min(len, free);//我其实可以选择丢弃当前整个air包，但是考虑nagle模式下可能合并包，所以还是能读多少air读多少。
//     size_t firstPart = min(to_wri, A2W_RING_SIZE - a2wHead);  // 到末尾的空间

//     memcpy(a2wRing + a2wHead, data, firstPart);// 第一段：head → 末尾
//     if (to_wri > firstPart) memcpy(a2wRing, data + firstPart, to_wri - firstPart);// 第二段：绕回开头（如果需要）
//     a2wHead = (a2wHead + to_wri) & A2W_RING_SIZE_SUB_ONE; 
//     return to_wri;
// }


// ---------------------------------------------------------
// ✅ 优化 1：直接从 AIR 读取到 Ring Buffer (消灭 tempBuf)
// ✅ 之前的需要修改，因为 环形buf满了以后应该是新数据覆盖旧数据
// ---------------------------------------------------------
// size_t checkRingSpace(size_t &len) {
//     if (len == 0)return 0;
//     if (len > A2W_RING_SIZE_SUB_ONE) len = A2W_RING_SIZE_SUB_ONE; // 如果大于整个缓冲区就丢掉后面的
//     ringLack = len - a2wFree();
//     if(ringLack > 0){
//         a2wTail = (ringLack + a2wTail) & A2W_RING_SIZE_SUB_ONE; // 更新 tail 丢弃最旧的数据
//     }
//     size_t firstPart = min(len, A2W_RING_SIZE - a2wHead);  // 到末尾的空间
//     return firstPart;
// }

// ---------------------------------------------------------
// ✅ 应对突发大包，情愿丢后续完整的包，也要读完。
//    但是通常不会，pbuf 链表够用，大包不会连续来
// ---------------------------------------------------------
// ESP32 WiFi 底层：
// ┌──────────────────────────────┐
// │  UDP 接收队列（pbuf 链表）    │
// │  默认：~5-8 个 pbuf          │
// │  每个 pbuf：~1.5KB           │
// │  总计：~8-12KB               │
// └──────────────────────────────┘
// 来了新包 → 队列满了 → 直接丢弃！不覆盖！

// 如果len大于free，就只写入free大小，并不更新udp.parsePacket()。下一个循环继续读当前包
int16_t checkRingSpace(int16_t &len) { //引用
    if (len < 0 ) len = 0; //重复保险 //如果len小于0，说明上次包已经读完了，或者根本就没有包了，就不读了。
    if (len == 0)return -1; 
    a2wToWrite = a2wFree();
    ringLack = len - a2wToWrite; // 计算缺少的空间，正数表示超出部分（需要丢弃的旧数据），负数表示剩余空间
    if (a2wToWrite == 0) return -1; //如果没有空间了，就不读了。
    a2wToWrite = min((size_t)len, a2wToWrite); //如果len大，写入free大小，如果len小，写入len大小
    int16_t firstPart = min(a2wToWrite, A2W_RING_SIZE - a2wHead);  // 到末尾的空间
    return firstPart;
}





// 从环形缓冲区读取（写入 Serial 时调用）
// size_t a2wRingRead(uint8_t* data, size_t maxLen) {
//     size_t readCount = 0;
//     while (readCount < maxLen && a2wHead != a2wTail) {
//         data[readCount++] = a2wRing[a2wTail];
//         a2wTail = (a2wTail + 1) % A2W_RING_SIZE;
//     }
//     return readCount;
// }

// size_t readRing_writeToSerial(uint8_t* data, size_t maxLen) {
//     size_t used = a2wUsed();
//     if (used == 0) return 0;
//     size_t toRead = min(maxLen, used); 
//     size_t firstPart = min(toRead, A2W_RING_SIZE - a2wTail);  // 到末尾的数据

//     memcpy(data, a2wRing + a2wTail, firstPart);// 第一段：tail → 末尾
//     if (toRead > firstPart)  memcpy(data + firstPart, a2wRing, toRead - firstPart);// 第二段：绕回开头（如果需要）
//     a2wTail = (a2wTail + toRead) & A2W_RING_SIZE_SUB_ONE;
//     return toRead;
// }

// // 环形缓冲区 到 临时缓冲区 到 串口, 主循环高频调用，模拟流，每次写入serial fifo可用空间
// void RingToSerial() {
//     int16_t s_tx_spc = S_DATA->availableForWrite();
//     if (s_tx_spc > 0 && a2wUsed() > 0) {
//         size_t toRead = min((size_t)s_tx_spc, min(a2wUsed(), sizeof(a2wBuf)));
//         size_t len = readRing_writeToSerial(a2wBuf, toRead);
//         if (len > 0) {
//             S_DATA->write(a2wBuf, len);    //已有a2wBuf临时缓冲区
//         }
//     }
// }

// ---------------------------------------------------------
// ✅ 优化 2：直接从 Ring Buffer 写入 Serial (消灭 a2wBuf)
// ---------------------------------------------------------
void RingToSerial() {
    int s_tx_spc = S_DATA->availableForWrite(); // 硬件 FIFO 剩余空间
    size_t ringUsed = a2wUsed();            // 环形缓冲数据量
    
    if (s_tx_spc > 0 && ringUsed > 0) {
        
        size_t to_wri = min((size_t)s_tx_spc, ringUsed);// 这次能发多少：取 硬件空位 和 现有数据 的最小值
        size_t firstPart = min(to_wri, A2W_RING_SIZE - a2wTail); // 计算第一段（Tail 到 数组末尾）的长度
        
        S_DATA->write(a2wRing + a2wTail, firstPart);// 1️⃣ 第一写：发送尾部数据
        
        if (to_wri > firstPart)     S_DATA->write(a2wRing, to_wri - firstPart);// 2️⃣ 第二写：如果有回卷，发送开头数据
        
        a2wTail = (a2wTail + to_wri) & A2W_RING_SIZE_SUB_ONE;// 更新 Tail (位运算优化)
    }
    
}





// ========== W2A线性缓冲区定义 ==========
void getMaxPayLoad(){ //    放setup/init里
    if (crnt_Mode == MODE_BLE) {
        MODE_PAYLOAD_MAX = min(BLE_PAYLOAD_MAX, w2aSize);  // 限制为 500?，防止截断
    } else if (crnt_Mode == MODE_SPP) {
        MODE_PAYLOAD_MAX = min(SPP_PAYLOAD_MAX, w2aSize); //限制为 330? 以内，虽然防止 IP 分片时阻塞
        //没有availableForWrite让我判断，配合ringBuf。
    } else if (crnt_Mode == MODE_UDP) {
        MODE_PAYLOAD_MAX = min(UDP_PAYLOAD_MAX, w2aSize); //限制为 1472 以内，防止IP分片
    } else if (crnt_Mode == MODE_TCP){
        MODE_PAYLOAD_MAX = w2aSize;  // TCP应用层为流式操作，不限制包大小。
    }
    else{
        MODE_PAYLOAD_MAX = w2aSize;  // 默认
    }
}
//uart发送需要线路电平时间，很耗时间，除非Baud率极高。但是uart读取是直接来自fifo，不耗时间，一次读完
//while循环耗时间，所以不用read()。用read(buf,size),读完就继续。而readByte(buf,size)会强制读取size大小，会阻塞
// 串口 到 线性缓冲区，阻塞式，因为必须把fifo读完才不会丢数据
void serialToW2aBuf() {
    int16_t uartRxDatSize = S_DATA->available();
    if (uartRxDatSize > 0 && w2aLen < MODE_PAYLOAD_MAX) //没必要阻塞式循环读完，因为若此时是9600baud，就卡很久。主循环高频轮询就行
    // while (uartRxDatSize > 0 && w2aLen < MODE_PAYLOAD_MAX)
    { 
        int len = S_DATA->read(w2aBuf + w2aLen, min(uartRxDatSize, (int16_t)(MODE_PAYLOAD_MAX - w2aLen)));
        if (len > 0)
        {
            w2aLen += len;
        }
        // uartRxDatSize = S_DATA->available(); //没必要阻塞式循环读完
    }
    // // 逐字节读
    // while (uartRxDatSize > 0 && w2aLen < MODE_PAYLOAD_MAX) {
    //     w2aBuf[w2aLen] = (uint8_t)S_DATA->read();
    //     w2aLen += 1;
    //     uartRxDatSize = S_DATA->available(); //不能用 uartRxDatSize -= 1因为可能期间又有数据来
    // }
    
}

// 判断 Flush 条件
bool shouldFlush(){
    // 检查flush条件，因为无线协议都有包头，所以payload没必要太小
    unsigned long elapsedUs = micros() - lastFlushUs;
    // flush逻辑：防止数据卡住
    bool ok2Flush = w2aLen >= FLUSH_MIN_SIZE && elapsedUs >= FLUSH_MIN_TIME_US &&
    (w2aLen >= FLUSH_THRES_SIZE || elapsedUs >= FLUSH_THRES_TIME_US);
    return ok2Flush;
}