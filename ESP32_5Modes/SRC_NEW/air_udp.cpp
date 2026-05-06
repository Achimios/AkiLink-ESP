#include "air_udp.h"

#include "buf_rules.h"
#include "data_config.h"
#include "wifi_config.h"

struct udp_pcb* air_udp_pcb = NULL;
uint16_t target_port = 0;
bool udp_target_locked = false;

ip_addr_t target_ip;         // 存储锁定的目标 IP
ip_addr_t udp_broadcast_ip;  // 广播地址



// #define _RUN_PRESSURE_TEST  //  █▓▒░░ 极限测试 ░░▒▓█ 🔴

#ifdef _RUN_PRESSURE_TEST
#define BULLET_COUNT 10
struct pbuf* magazine[BULLET_COUNT];
int current_bullet = 0;

// 2. 在 setup 里初始化，先把表单领好
void prepare_magazine() {
  for (int i = 0; i < BULLET_COUNT; i++) {
    // 预先分配 ROM 类型的 pbuf，指向你的 w2aBuf
    magazine[i] = pbuf_alloc(PBUF_TRANSPORT, JUNK_SIZE, PBUF_ROM);
    if (magazine[i] != NULL) {
      magazine[i]->payload = w2aBuf;  // 强行指向你的蒙娜丽莎
    }
  }
}
#endif
#ifdef _TIPS_
struct pbuf {
  struct pbuf* next;  // 链表指针：如果是长包，指向下一截
  void* payload;      // 核心：指向数据存放的地址 (你的 w2aBuf)
  u16_t tot_len;      // 总长度
  u16_t len;          // 当前这一截的长度
  u8_t type;          // 类型 (PBUF_ROM, PBUF_RAM 等)
  u8_t flags;         // 标志位
  u16_t ref;          // 【关键】引用计数器 (Reference Count)
  // alloc的初始化是1, pbuf_ref会+1(如果别处的TCP也要用它)，pbuf_free会-1
};
#endif



void update_broadcast_ip() {
  if (isAPMode) {
    // AP 模式固定广播地址 10.0.0.255
    IP_ADDR4(&udp_broadcast_ip, 10, 0, 0, 255);
  } else {
    // STA 模式：根据当前 IP 和掩码计算
    uint32_t ip = (uint32_t)WiFi.localIP();
    uint32_t mask = (uint32_t)WiFi.subnetMask();
    uint32_t brd = ip | (~mask);

    // 转换回 lwIP 格式 (注意大小端转换)
    udp_broadcast_ip.u_addr.ip4.addr = brd;
    udp_broadcast_ip.type = IPADDR_TYPE_V4;
  }
}

/**
 * @brief UDP 接收回调 (核心：Air -> a2wRing)
 */

static void udp_recv_cb(void* arg, struct udp_pcb* pcb, struct pbuf* p, const ip_addr_t* addr, u16_t port) {
  if (p == NULL) return;
  // 第一次
  if (!udp_target_locked) {
    target_ip = *addr;   // 锁定第一个发送者为目标
    target_port = port;  // 上位机大概率监听自己的发送Port
    udp_target_locked = true;
    DEBUG_PRINTLN("[UDP] 锁定目标 IP: " + String(ipaddr_ntoa(addr)));
  } else {
    if (!ip_addr_cmp(&target_ip, addr) && !LISTEN_BROAD)  // || target_port != port
    {
      DEBUG_PRINTLN("⚠️ 非配对 IP，丢 UDP 包");
      pbuf_free(p);
      return;
    }
  }

  //*1️1️1️1️== airRX → a2w环形Buf    ==1️1️1️1️*
  // 2. 数据搬运 (Air -> a2wRing)
  struct pbuf* q = p;
  while (q != NULL) {
    packetSize = (int16_t)q->len;  // 使用你的全局变量
    int16_t first_part = checkRingSpace(packetSize);
    if (first_part >= 0) {
      memcpy(a2wRing + a2wHead, q->payload, first_part);
      if (a2wToWrite > first_part) memcpy(a2wRing, (uint8_t*)q->payload + first_part, a2wToWrite - first_part);
      a2wHead = (a2wHead + a2wToWrite) & A2W_RING_SIZE_SUB_ONE;
    } else {
      DEBUG_PRINTLN("UDP 环形缓冲区已满，丢弃包");
      break;  // 环形缓冲区满了，后续数据不处理了（丢包），直接跳出循环，等主循环发完Serial后再来读UDP包
    }
    q = q->next;  // 处理下一个 pbuf 节点,
    // lwip/udp.h 不像 <WiFiUdp.h>
    // 会在堆上分配一个大缓冲区来存整个UDP包，所以需要遍历 pbuf
    // 链表来处理完整的UDP数据
    //<WiFiUdp.h>
    // 会new在堆上，不如直接增大a2wRing来得快，毕竟内存操作比动态分配快多了。
  }
  pbuf_free(p);  // 释放 pbuf
}


//   ▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀█
//   █  ⬒   ⬓   ⬔   ⬕   ◆   ◇   ◈   ◉   ◊   ○   ●   ◐   ◑  █
//   █  ◼  ◻  ◾  ◽  ▢  ▣    BEGIN   ▧  ▨  ▩  ▪  ▫  ▬  ▭  ▮  █
//   ▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
void airUdp_begin() {

#ifdef _RUN_PRESSURE_TEST
  prepare_magazine();
#endif


  DEBUG_PRINTLN("[UDP] 初始化 (lwIP Raw 模式)...");

  if (!connectToSTA()) { startAPMode(); }

  // 创建 UDP PCB
  update_broadcast_ip();  // 初始化广播地址
  target_port = PC_UDP_PORT;  // 预设目标端口
  air_udp_pcb = udp_new();
  if (air_udp_pcb != NULL) {
    // 绑定本地端口
    err_t err = udp_bind(air_udp_pcb, IP_ADDR_ANY, ESP32_UDP_PORT);
    if (err == ERR_OK) {
      // 注册接收回调
      udp_recv(air_udp_pcb, udp_recv_cb, NULL);
      DEBUG_PRINTLN("[UDP] 监听端口: " + String(ESP32_UDP_PORT));
    } else {
      DEBUG_PRINTLN("[UDP] 绑定失败");
      udp_remove(air_udp_pcb);
      air_udp_pcb = NULL;
    }
  }
}



#ifdef _RUN_PRESSURE_TEST
// ████████████████████████████████████████████████████████████
// █  ⬒   ⬓   ⬔   ⬕   ◆   ◇   ◈   ◉   ◊   ○   ●   ◐   ◑   ◼  █
// █              ------ ·   UPDATE   · --------             █
// █  ◼  ◻  ◾  ◽  ▢  ▣  ▤  ▥  ▦  ▧  ▨  ▩  ▪  ▫  ▬  ▭  ▮  ▯  █
// ████████████████████████████████████████████████████████████
// 🚀 JUNK 压测模式（带统计）
void airUdp_update() {
  // 1. 初始化垃圾数据（只运行一次）
  static bool fillBuf = false;
  if (!fillBuf) {
    // 假设 JUNK_SIZE 已经在外面定义好了（比如 1024 或 1460）
    memset(w2aBuf, '~', JUNK_SIZE);
    fillBuf = true;
    DEBUG_PRINTLN(">>> 系统就绪：JUNK 数据已填充，等待串口数字输入...");
    delay(500);
  }

  // 2. 静态变量定义
  static int16_t sendCounter = 0;           // 剩余发送次数
  static uint32_t totalBytesSent = 0;       // 统计：总字节
  static uint32_t roundBytes = 0;           // 统计：当前秒字节
  static unsigned long testStartTime = 0;   // 统计：开始时间
  static unsigned long lastReportTime = 0;  // 统计：上次报告时间
  static bool isTesting = false;            // 标记是否正在测试中

  // 3. 串口读取逻辑 (懒人版：有数据就读，转成数字)
  // 3. 串口读取逻辑 (明日香强化版：支持 C, J, T 指令)
  if (sendCounter == 0) {
    size_t availableRead = 0;
    uart_get_buffered_data_len(NUM_S_DEBUG, &availableRead);

    if (availableRead > 0) {
      delay(20);

      uint8_t tempBuf[16] = {0};  // 稍微加大一点，防止长指令溢出
      int len = uart_read_bytes(NUM_S_DEBUG, tempBuf, 15, 0);

      if (len > 0) {
        char type = tempBuf[0];
        // 兼容你原来的纯数字输入，或者显式 C 开头
        int val = (type >= '0' && type <= '9') ? atoi((char*)tempBuf) : atoi((char*)(tempBuf + 1));

        if (type == 'J' || type == 'j') {
          // 修改 JUNK_SIZE: 限制 0 ~ 20000
          JUNK_SIZE = constrain(val, 1, 20000);
          DEBUG_PRINTLN(">>> [PARAM] JUNK_SIZE 设为: " + String(JUNK_SIZE));
        } else if (type == 'I' || type == 'i') {
          // 修改 FLUSH_THRES_TIME_US: 限制 10 ~ 50000 us
          // 注意：这里假设你的变量名是 FLUSH_THRES_TIME_US
          FLUSH_THRES_TIME_US = constrain(val, 10, 50000);
          DEBUG_PRINTLN(">>> [PARAM] 发射间隔设为: " + String(FLUSH_THRES_TIME_US) + " us");
        } else if (val > 0 && val <= 10000) {
          // 默认指令或 C 指令: 开始发射
          sendCounter = val;
          testStartTime = micros();
          lastReportTime = micros();
          totalBytesSent = 0;
          roundBytes = 0;
          isTesting = true;

          DEBUG_PRINTLN(">>> 收到指令: 发射 " + String(val) + " 次");
          DEBUG_PRINTLN(">>> 全速模式 (JunkSize:" + String(JUNK_SIZE) + " Interval:" + String(FLUSH_THRES_TIME_US) +
                        "us) GO!");
        } else {
          DEBUG_PRINTLN("输入如 99 设置发射次数（0~10k次）");
          DEBUG_PRINTLN("输入如 j1472 设置包大小（1~20kB）");
          DEBUG_PRINTLN("输入如 i500 设置间隔微秒（10~50k）（100kHZ~20HZ）");
          DEBUG_PRINTLN(" ...目前...JunkSize:" + String(JUNK_SIZE) + " Interval:" + String(FLUSH_THRES_TIME_US) + "us");
        }
      }
      uart_flush_input(NUM_S_DEBUG);
    }
  }

  // 4. 发送逻辑 (全速狂奔模式)
  // 只要有次数 && 连上了WiFi && UDP初始化了
  checkWifiConnection();
  if (shouldFlushJunk() && sendCounter > 0 && deviceConnected && air_udp_pcb != NULL) {
    // 零拷贝分配 PBUF_ROM
    // struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, JUNK_SIZE, PBUF_ROM); //分配极其耗时
    struct pbuf* p = magazine[current_bullet];

    if (p != NULL) {
      // p->payload = w2aBuf;  // 指向静态垃圾数据 //已用预分配

      err_t err = udp_sendto(air_udp_pcb, p, &udp_broadcast_ip, PC_UDP_PORT);

      // pbuf_free(p);// 释放头信息 //预分配无需释放

      if (err == ERR_OK) {
        current_bullet = (current_bullet + 1) % BULLET_COUNT;
        sendCounter--;

        totalBytesSent += JUNK_SIZE;  // 累加统计数据
        roundBytes += JUNK_SIZE;

        lastFlushUs = micros();  // 更新活跃时间
      } else {
        DEBUG_PRINTLN("错误码: " + String(err));  // 注释以后即使失败也保持极限速度
        // vTaskDelay(10);
        delay(10);  // 会自动使用 vTaskDelay
        // taskYIELD(); //没创建其他vtask并行于主loop，所以什么也不会发生
        // yield(); // 会自动使用 taskYIELD
      }
    } else {
      DEBUG_PRINTLN("⚠️ pbuf_alloc 失败");  // 注释以后即使失败也保持极限速度
    }
  }

  // 5. 统计报告逻辑 (每秒汇报 + 结束汇报)
  if (isTesting) {
    unsigned long now = micros();

    // A. 每秒汇报一次瞬时速度
    if (now - lastReportTime >= 1000000) {
      float durationSec = (now - lastReportTime) / 1000000.0f;
      float speedMbps = (roundBytes * 8.0f) / (durationSec * 1000000.0f);

      DEBUG_PRINTLN(">> [进行中] 剩余: " + String(sendCounter) + " | 速度: " + String(speedMbps, 7) + " Mbps");

      // 重置这一轮的数据
      roundBytes = 0;
      lastReportTime = now;
    }

    // B. 彻底结束时的总结
    if (sendCounter == 0) {
      float totalDurationSec = (now - testStartTime) / 1000000.0f;
      float avgSpeedMbps = (totalBytesSent * 8.0f) / (totalDurationSec * 1000000.0f);

      DEBUG_PRINTLN("\n====== 🏁 测试报告 🏁 ======");
      DEBUG_PRINTLN("总耗时: " + String(totalDurationSec, 7) + " 秒");
      DEBUG_PRINTLN("总流量: " + String(totalBytesSent / 1024.0 / 1024.0, 7) + " MB");
      DEBUG_PRINTLN("平均速: " + String(avgSpeedMbps, 7) + " Mbps");
      DEBUG_PRINTLN("==========================\n");

      isTesting = false;  // 停止统计
    }
  }
}

#else
// //+++++++++++++++++++上面是测试代码++++++++++++++++++++++
// //-------------------下面是正式版 --------------------------
// ████████████████████████████████████████████████████████████
// █  ⬒   ⬓   ⬔   ⬕   ◆   ◇   ◈   ◉   ◊   ○   ●   ◐   ◑   ◼  █
// █              ------ ·   UPDATE   · --------             █
// █  ◼  ◻  ◾  ◽  ▢  ▣  ▤  ▥  ▦  ▧  ▨  ▩  ▪  ▫  ▬  ▭  ▮  ▯  █
// ████████████████████████████████████████████████████████████

void airUdp_update() {
  checkWifiConnection();

  // 1. Air -> Wire (SerialTX)
  // 逻辑已在回调函数中自动完成，这里只需触发串口发送
  //*2️2️2️2️== a2w环形Buf → SerialTX ==2️2️2️2️*
  RingToSerial();

  // 2. Wire -> Air (UDP TX)
  if (!deviceConnected) {  // deviceConnected是wifi连接
    // DEBUG_PRINTLN("WiFi 未连接");
    return;
  }
  if (air_udp_pcb == NULL) {  // 确保 UDP 控制块已经创建 && 已经锁定了目标 IP
    // DEBUG_PRINTLN("UDP 未初始化");
    return;
  }


  //*3️3️3️3️== SerialRX → w2a线性Buf ==3️3️3️3️*
  serialToW2aBuf();  // 读取串口到 w2aBuf

// #define _SPEED_TEST
#ifdef _SPEED_TEST  // 发送速率统计
  static uint32_t byteThisRound = 0;    // 本轮从串口读了多少字节
  static bool stayInThisRound = false;  // 直到串口数据不连续了才重置
  static unsigned long lastRoundUs;     // 串口流式传输没包的概念，因为每包就是1B,10bit。每轮计时
  static unsigned long elapsedRoundUs;
  static unsigned long lastStepUs;  // rx多久没更新就认为是下一轮
  static bool nowZeroLen = false;   // 当前是否处于 w2aLen == 0 的状态
  if (w2aLen > 0) {
    lastStepUs = micros();  // ?ms没更新，就认为是下一轮
    nowZeroLen = false;
  }
  if (w2aLen > 0 && !stayInThisRound) {
    lastStepUs = micros();
    lastRoundUs = micros();
    stayInThisRound = true;
  } else if (stayInThisRound && w2aLen == 0)  // stayInThisRound确保执行一次，w2aLen
                                              // == 0 确保数据发完，
  {
    if (!nowZeroLen) elapsedRoundUs = micros() - lastRoundUs;
    nowZeroLen = true;
    // if ((micros() - lastStepUs) <= (200.0f * 1000000 / (BAUD_DATA / 10)))
    // //麻了，USB转TTL会有停顿导致误判断 //当前波特率下每B的传输时间的 ?
    // 倍是连续性判断最大间隔
    if ((micros() - lastStepUs) <= 100000)  // 100ms。该死的usb TTL,我暂时不得不设置100ms
      goto end;
    delay(100);
    DEBUG_PRINTLN("本轮字节数: " + String(byteThisRound));
    DEBUG_PRINTLN("本轮耗时: " + String(elapsedRoundUs / 1000.0f, 3) + " ms");
    DEBUG_PRINTLN("平均速率: " + String(1000.0f * byteThisRound / elapsedRoundUs, 3) + " KB/s");
    DEBUG_PRINTLN("因为第一次已经存入缓存，所以实际速率会比最大速率略高");
    DEBUG_PRINTLN("理论最大速率(被串口限制)" + String(BAUD_DATA / 10000.0f, 3) + " KB/s");
    stayInThisRound = false;
    byteThisRound = 0;
  end:;
  }
#endif

  //*4️4️4️4️== w2a线性Buf → AirTX    ==4️4️4️4️*
  if (shouldFlush()) {
    DEBUG_PRINTLN("Flush~");

    // 创建一个新的 pbuf 用于发送   PBUF_TRANSPORT: 预留 UDP/IP 首部空间
    // PBUF_RAM:
    // 创建所有网络包都要求的连续内存缓存，所以Ring很容易被拆为2段，因为你无法像控制LORA一样控制
    // tx go 当你调用 udp_sendto 时，lwIP 实际上是将这个 pbuf
    // 的所有权交给了底层驱动。pbuf_free 只是把引用计数减
    // 1。如果底层驱动还没发完，它会持有这个
    // pbuf，直到数据真正送入硬件寄存器后才真正释放。所以 PBUF_RAM 模式下，你
    // pbuf_free 后立刻清空 w2aBuf 是绝对安全的。
    struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, w2aLen,
                                PBUF_RAM);  // if (p = NULL)return; //没必要检测
    memcpy(p->payload, w2aBuf,
           w2aLen);  // 1000B 约 1.5-3 us。100B 约 0.2-0.5 us。
    // struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, w2aLen, PBUF_REF); //
    // 危险，即使pbuf_free(p);后可能还在被 lwIP
    // 发送线程访问，因为我们还没有真正把数据交给 lwIP，lwIP 还没有机会把这个
    // pbuf 标记为已发送并释放它。这期间不能修改 w2aBuf p->payload = w2aBuf;
    // // 直接指向你的线性缓冲区

    // 目标决策    // 如果 锁定目标IP && 非广播，发送到广播地址
    const ip_addr_t* dest_ip = (udp_target_locked && !SEND_BROAD) ? &target_ip : &udp_broadcast_ip;
    err_t err = udp_sendto(air_udp_pcb, p, dest_ip, target_port);
    DEBUG_PRINTLN("IP:" + String(ipaddr_ntoa(dest_ip)) + " Port:" + String(target_port));
    // err_t err = udp_sendto(air_udp_pcb, p, &udp_broadcast_ip, PC_UDP_PORT);
    pbuf_free(p);       // 发送完立刻释放
    if (err == ERR_OK)  // 如果失败了，保留w2aLen,下次再试。虽然几乎不可能失败因为内存不可能满
    {
#ifdef _SPEED_TEST
      byteThisRound += w2aLen;
#endif
      w2aLen = 0;
      lastFlushUs = micros();
      DEBUG_PRINTLN("Air Out~~");
    } else {
      DEBUG_PRINTLN("UDP 发送失败，错误码: " + String(err));
    }
  }
}
#endif