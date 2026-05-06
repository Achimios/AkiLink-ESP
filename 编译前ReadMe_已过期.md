
# 已过期，很多地方需要修改，如C3已转移到单独项目

# 🚀 ESP32 多协议透传/图传项目维护指南

> **Note:** 本项目代码包含大量协议解析注释及部分弃用逻辑，修改前请仔细阅读源码注释。

------

### ⚡ 快速开始 (Hardware QuickStart)

- 你只需要一个 **ESP32 开发板**，默认将 **32 引脚** 接上开关（用于配置/功能切换）即可直接使用。
- 考虑到体积，建议使用 **ESP32 裸模块** 搭配 **DC-DC 或 LDO** 自制 PCB。
- 引脚配置都在 `data_config.h` 中。

------

## 🛠️ 核心配置说明

| **配置文件**     | **关键作用**       | **备注**                                       |
| ---------------- | ------------------ | ---------------------------------------------- |
| `data_config.h`  | **全局参数**       | 发布前请务必 **注释** `#define _FACTORY_DEBUG` |
| `buf_rules.h`    | **缓存与发包规则** | 定义固定发包长度、超时时间等参数               |
| `wifi_config.h`  | **网络基础配置**   | 包含 SSID、AP 模式 IP 地址等                   |
| `platformio.ini` | **编译选项**       | 建议设置 `-D CORE_DEBUG_LEVEL=0` 以提升性能    |
BLE和SPP名字在各自.h里
-------



## 📡 双ESP通讯时 怎么用
- **Wi-Fi 模式**：推荐使用 **STA-AP 互联**。任意一端接 PC 模拟串口即可，带宽足够满足图传需求。
- **混合互联**：暂不实现 SPP 与 BLE 的互联，Wi-Fi 性能已足够应对目前场景。
- **ESP-NOW**：还没空写
------

## ⚙️ 架构与性能优化
- **驱动模式**：采用**主循环高频轮询**。所有任务基于条件判断和 Timer 开启，未使用中断。
- 若主循环要处理重型任务，可重构为 `Timer + ISR + DMA` ，但代码量将翻倍且逻辑复杂度激增。
- 若使用esp-idf架构，看看能不能开启 serial tx soft ring buf，目前只能开 rx 的
- 也许没必要也不建议用多核

- **SPI 扩展**：
  - 替换 UART 对象为 SPI，必须启用 **DMA**。
  8位❌️，16位✅️————软件spi❌️，硬件spi✅️————noDMA❌️，DMA✅️
  dma,  dma eof isr,  exti(vsync)
  - 硬件上需额外拉一条 **Data Ready** 信号线用于从机主动通知。
- ⚠️ **Watchdog (看门狗) 警报**：
  > 由于采用轮询架构且关闭了部分中断任务，需确保单次 Loop 耗时 **< WDT 阈值** (一般 1-5s)。处理 SPP/BLE 大包时，若出现卡顿请手动插入 `yield()` 或 `vTaskDelay(1)`。

- 这是你在做 SPI 扩展或图传时最需要关注的点：

| **特性**     | **IOMUX (直连)**   | **GPIO Matrix (矩阵路由)**          |
| ------------ | ------------------ | ----------------------------------- |
| **路径**     | 直接连接引脚       | 经过一层映射逻辑                    |
| **延迟**     | 极低（纳秒级）     | 稍微增加（约 1-2 个时钟周期）       |
| **最高速率** | **最高支持 80MHz** | **最高通常限制在 26.6MHz 或 40MHz** |
| **灵活性**   | 固定引脚，不能乱选 | 极其灵活，任意引脚皆可              |

------

## 🧪 自动化测试套件
项目配套有 Python 测试脚本，用于统计**吞吐速率**、**丢包率**及**误码率**：
- ⌨️ **串口端**：`test_UART.py`
- 🌐 **无线端**：
  - `test_TCP.py` / `test_UDP.py` (推荐)
  - `test_SPP.py` / `test_BLE.py`

------

### 💡 进阶研究课题
1. **PHY层wifi注入**：研究 802.11 注入下的最大实际速率。
### 🌀👃🌀 无聊研究课题
1. **ESP-NOW 极限测试**：研究最最小发包间隔（太小会失败，或丢包），最大单包大小，最大实际速率














-----------------------------------------------------------------------------------------------------------------------------
//   .·´¯`·.´¯`·.¸¸.·´¯`·.¸><(((º>´¯`·.´¯`·.¸¸.·´¯`·.¸><(((º>
//   ╭━━╮╭━━┳━━┳━━┳━╮╭━┳━━╮╭━━┳━━┳━╮╭━┳━━╮╭━━┳━━┳━━╮╭━━╮
//   ┃╭╮┃┃╭╮┃╭╮┃╭╮┃┃╰╯┃┃╭╮┃┃╭╮┃╭╮┃┃╰╯┃┃╭╮┃┃╭╮┃╭╮┃╭╮┃┃╭╮┃
//   ┃╰╯╰┫╰╯┃╰╯┃╰╯┃╭╮╭╮┃╰╯┃┃╰╯┃╰╯┃╭╮╭╮┃╰╯┃┃╰╯┃╰╯┃╰╯┃┃╰╯┃
//   ╰━━╯╰━━┻━━┻━━┻╯╰╯╰┻━━╯╰━━┻━━┻╯╰╯╰┻━━╯╰━━┻━━┻━━╯╰━━╯
//   `·.¸¸.·´¯`·.´¯`·.¸¸.·´¯`·.¸><(((º> `·.¸¸.·´¯`·.´¯`·.¸¸.



# 额外：
目前还是 Arduino 框架，如果要发挥极限速度，必须切换 PIO ESP-IDF 甚至 ESP-IDF 原生工具链，开启WIFI优化
在esp-idf中可改的
### 增加 WiFi 动态发送缓存（默认可能是 32，直接拉到 64 或 128）
CONFIG_ESP32_WIFI_DYNAMIC_TX_BUFFER_NUM=64
### 增加静态缓存（备弹）
CONFIG_ESP32_WIFI_STATIC_TX_BUFFER_NUM=16
### 关键：开启 WiFi AMPDU（帧聚合）窗口
### 这能让 WiFi 一次性把多个 UDP 包封装成一个超大帧发出去，效率翻倍
CONFIG_ESP32_WIFI_TX_BA_WIN=32

### 或直接用 PHY层wifi注入
### 伪代码：直接调用 WiFi 抽象层，发送原始数据
esp_wifi_80211_tx(WIFI_IF_STA, raw_packet_buffer, packet_size, true);


-----------------------------------------------------------------------------------------------------------------------------
//   ════════════════════════════════════════════════════════════
//   █▀▀ █▀▀█ █▀▀█ ▀▀█▀▀ ░▀░ █▀▀ █▀▀▄ █▀▀█ █░░█ █▀▀ █▀▀▄ 
//   █░░ █▄▄▀ █▄▄█ ░░█░░ ▀█▀ █▀▀ █░░█ █▄▄█ █▀▀█ █▀▀ █░░█ 
//   ▀▀▀ ▀░▀▀ ▀░░▀ ░░▀░░ ▀▀▀ ▀▀▀ ▀░░▀ ▀░░▀ ▀░░▀ ▀▀▀ ▀░░▀ 
//   ════════════════════════════════════════════════════════════
//   ▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄


#### 1. 撑大 WiFi 的喉咙 (Component config -> Wi-Fi)

- **`CONFIG_ESP32_WIFI_STATIC_TX_BUFFER_NUM`**: 改为 **16** (默认可能只有 6-10)。
- **`CONFIG_ESP32_WIFI_DYNAMIC_TX_BUFFER_NUM`**: **暴力拉到 64** (默认 32)。这就是为什么你发 30 包就挂的原因，池子太浅！
- **`CONFIG_ESP32_WIFI_TX_BA_WIN`**: 改为 **32** (默认可能 6)。**这个最关键！** 这决定了 WiFi 一次能聚合多少个包发出去。设为 32 意味着一次“突突突”发 32 帧才需要对面回一个话。
- **`CONFIG_ESP32_WIFI_AMPDU_TX_ENABLED`**: **必须 Enable**。
- **`CONFIG_ESP32_WIFI_AMPDU_RX_ENABLED`**: **必须 Enable**。

#### 2. 优化 LwIP 内存 (Component config -> LWIP)

- **`CONFIG_LWIP_IRAM_OPTIMIZATION`**: **Enable**。把 LwIP 的核心代码放进 IRAM（指令 RAM），让 CPU 读取指令零延迟。
- **`CONFIG_LWIP_UDP_SEND_BUF_DEFAULT`**: 加大，建议 **65535**。
- **`CONFIG_LWIP_PBUF_POOL_SIZE`**: 建议 **32** 或更高。

### 三、 代码层：如何模仿 `iperf` 的稳？

既然你追求极致，有两个流派。

#### 流派 A：投降派（改用 BSD Socket，最稳）

直接用标准 Socket，让 OS 帮你管理“堵车”时的等待。这是最容易达到 40Mbps 持续发送的方案。

C

```
#include "lwip/sockets.h"

void iperf_style_fire() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    // ... bind, connect 等标准操作 ...

    // 关键：设置发送缓冲区，让 OS 帮你缓存更多
    int size = 64 * 1024; // 64KB
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));

    uint8_t payload[1472]; // 甚至不需要 ROM pbuf，直接栈上开
    
    while (1) {
        // 这里的 sendto 是阻塞的！
        // 如果底层满了，它会自动睡觉，绝不浪费 CPU 轮询
        int err = sendto(sock, payload, sizeof(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        
        if (err < 0) {
            // 只有真出大问题才报错，普通的“满”它内部就消化了
            vTaskDelay(10); 
        }
    }
}
```

#### 流派 B：死磕派（继续用 Raw LwIP，但要手动实现流控）

如果你非要用 `pbuf` 指针那一套（为了省内存拷贝），你需要手动实现“堵车时睡觉”。

你不能用 `while(err == ERR_MEM)` 死等，你要**让出时间片**。

C

```
// 1. 定义一个信号量或者简单的标志
volatile bool tx_congested = false;

void fire() {
    struct pbuf* p = magazine[current_bullet];
    
    // 疯狂循环
    while (1) {
        err_t err = udp_sendto(upcb, p, &target_ip, target_port);
        
        if (err == ERR_OK) {
            // 发送成功，切弹
            current_bullet = (current_bullet + 1) % BULLET_COUNT;
            p = magazine[current_bullet];
            // 成功时不 delay，全速推进！
        } 
        else if (err == ERR_MEM || err == ERR_WOULDBLOCK) {
            // 【关键点】
            // 遇到拥堵，绝对不要 while 空转！
            // 这里的 1 tick (1ms) 对于 40Mbps 来说可能太长了
            // 但如果不用 OS 阻塞，你很难做到微秒级精准让出
            
            // 方案：最简单的退避算法
            vTaskDelay(1); 
            
            // 进阶方案（如果你能改底层）：
            // 注册一个 LwIP 的 callback，等 buffer 有空位了通知你（太复杂不推荐）
        }
    }
}
```

------------------------------------------------------------------------------------------------------------------------------
// ╔══════════════════════════════════════════════════════════════╗
// ║     ▄▄▄·  ▄▄▄· ▄▄▄  ▄▄▄ .▐▄• ▄     ▄▄▄·  ▄▄▄· ▄▄▄▄▄ ▄ .▄    ║
// ║    ▐█ ▀█ ▐█ ▄█ ▐█ ▀█ ▀▄.▀· █▌█▌▪   ▐█ ▄█ ▐█ ▄█ •██  ██▪▐█    ║
// ║    ▄█▀▀█  ██▀· ▄█▀▀█ ▐▀▀▪▄ ·██·     ██▀·  ██▀·  ▐█.▪██▀▐█    ║
// ║    ▐█ ▪▐▌▐█▪·•▐█ ▪▐▌▐█▄▄▌▪▐█·█▌    ▐█▪·• ▐█▪·•  ▐█▌·██▌▐▀    ║
// ║     ▀  ▀ .▀    ▀  ▀  ▀▀▀ •▀▀ ▀▀    .▀    .▀     ▀▀▀ ▀▀▀ ·    ║
// ║                    ☢️ FALLOUT SHELTER ☢️                     ║
// ╚══════════════════════════════════════════════════════════════╝



我们分两部分来解答：

------

### 第一部分：关于“原始 802.11 帧”与图传

**是的，直接打原始 802.11 帧（Raw 802.11 Frame）通常被称为“WiFi 注入 (Packet Injection)”**。

在图传（Video Transmission）领域，特别是像 **OpenHD** 或 **RubyFPV** 这种开源远距离高清图传项目中，这是**标准操作**。

#### 1. 为什么图传要用它？

普通的 UDP/TCP 就像寄快递，你要填单子（IP头）、打包（UDP头）、检查（校验和）、确认收货（ACK）。 而 **Raw 802.11** 就像你站在楼顶直接往下扔砖头。

- **优点（极速）：**
  - **零协议头开销**：不需要 IP 头（20字节）和 UDP 头（8字节），省流量。
  - **零握手**：不需要像 TCP 那样建立连接，也不需要像标准 UDP 那样经过 LwIP 协议栈的处理。
  - **广播特性**：你可以利用 802.11 的广播机制，发一份数据，多个接收端（眼镜、手机、电脑）同时看，互不影响。
  - **单向传输**：如果是单向图传，接收端根本不需要发 ACK 回来，这消除了“半双工”带来的通信等待时间。
- **缺点（硬核）：**
  - **没有重传**：丢包了就丢了，画面会花一下，但不会卡顿（这对实时图传是好事，卡顿比花屏更致命）。
  - **没有路由**：路由器看不懂你的包，你只能点对点（P2P）或者在局域网内广播。
  - **兼容性差**：普通的手机 APP（基于 Socket）收不到这种包，你得把手机网卡切到 **监控模式 (Monitor Mode)** 才能收到，或者用另一块 ESP32 做接收端（网桥）。

#### 2. 代码实现思路

如果你要做图传，接收端和发送端都需要特殊配置：

- **发送端 (Camera Side):**

  C

  ```
  // 构造一个 802.11 数据帧（你需要自己填 Frame Control, Duration, Mac Addr 等）
  uint8_t raw_packet[1024] = { ... }; 
  // 直接把这一坨数据给 WiFi 硬件
  esp_wifi_80211_tx(WIFI_IF_STA, raw_packet, len, true);
  ```

- **接收端 (Display Side):** 需要开启 **混杂模式 (Promiscuous Mode)**。

  C

  ```
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&my_sniffer_callback);
  // 在 callback 里过滤出你的私有协议包，解析视频数据
  ```

------

### 第二部分：深度解读 IPERF 示例代码

你贴出的这段代码（`iperf_example_main.c`）实际上是一个 **“空壳” (Wrapper)**。真正的干货都被封装在 `wifi-cmd` 和 `iperf` 这两个组件里了。

这就是为什么你没看到 socket 操作的原因——它把脏活累活都藏在库里了。

我们像剥洋葱一样把它剥开：

#### 1. 核心流程解析

这段 `app_main` 只做了三件大事：

1. **环境搭建 (Setup):**
   - `nvs_flash_init()`: 初始化存储，WiFi 驱动需要读 NVS 里的校准数据。
   - `wifi_cmd_initialize_wifi(NULL)`: **这是关键点 1**。它封装了 `esp_wifi_init`, `esp_wifi_set_mode` 等繁琐步骤。
   - `esp_wifi_set_ps(WIFI_PS_NONE)`: **这是关键点 2**。**关闭省电模式**。如果不加这一行，ESP32 会在没有数据时频繁关闭射频电路来省电，导致高吞吐时延迟爆炸。做图传必须加这行！
2. **钩子挂载 (Hooking):**
   - `app_register_iperf_hook_func(...)`: 当 iperf 开始或结束时，自动执行这个回调。
   - 回调里的 `wifi_cmd_get_tx_statistics`: 这会调用底层 API 去查询 WiFi 硬件寄存器，告诉你发了多少包、丢了多少包、AMPDU 聚合成功率是多少。
3. **启动控制台 (REPL):**
   - `esp_console_start_repl`: 启动一个类似 shell 的交互界面，让你能输入 `iperf -s -u` 这样的命令。
   - `app_register_iperf_commands()`: **这是关键点 3**。它把字符串 `"iperf"` 绑定到了具体的 C 函数执行逻辑上。

#### 2. 没暴露的模块里到底写了啥？

虽然你看不到源码（除非你去 components 目录翻），但我可以告诉你 `iperf.c` 内部的逻辑，它用的就是我们之前讨论的 **“流派 A：BSD Socket 阻塞模式”**。

它的伪代码逻辑大约是这样的（位于 `components/iperf/iperf.c`）：

C

```
// 这是一个独立的 Task，通过命令行触发
void iperf_tx_task(void *arg) {
    // 1. 创建标准 Socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    // 2. 关键优化：设置 Socket 发送缓冲区 (SNDBUF)
    // 这让 OS 内核能帮你暂存更多数据，而不是立刻报错
    int buffer_size = 65536; 
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buffer_size, ...);

    // 3. 构造数据包（通常是随机 junk data）
    uint8_t *data = malloc(buffer_len);

    while (is_running) {
        // 4. 发送！
        // 注意：这是阻塞调用。如果 WiFi 忙，sendto 会让当前 Task 睡觉 (Block)
        // 绝不会像 udp_sendto 那样返回错误让你去 while 空转
        int ret = sendto(sock, data, buffer_len, 0, ...);
        
        if (ret < 0) {
            // 只有真出错（比如链路断了）才处理
            // 拥塞控制完全交给 TCP/IP 栈和 FreeRTOS
        }
        
        // 统计流量...
    }
}
```

### 给你的建议：图传该怎么走？

1. **如果只追求速度（极客方案）：** 走 **Raw 802.11 (`esp_wifi_80211_tx`)**。
   - 你需要自己定义包结构（比如：帧头+序号+视频数据）。
   - 你需要处理分包（一帧视频几十KB，一个 WiFi 包最多 1400B，你要自己切，自己组）。
   - **坑：** 这种包在复杂的 WiFi 环境下（比如商场）很容易被干扰，且没有自动重传机制，画面会花。
2. **如果追求稳妥（工程方案）：** 继续用 **UDP**，但按照 `iperf` 的方式写。
   - 使用 `socket()` API 而不是 raw lwip。
   - 修改 `sdkconfig` 增大缓冲区。
   - 开启 `AMPDU`。
   - 这样你可以直接用电脑上的 VLC 播放器（打开网络串流 `udp://...`）来接收和调试，开发效率极高。
3. **中间路线（ESP-NOW）：** ESP-NOW 本质上就是官方封装好的“Raw 802.11”，但加了简单的配对和确认机制。
   - **优点：**API 极其简单，支持广播，延迟极低。
   - **缺点：**每包 payload 只有 250 字节，对于发视频来说，拆包组包的 CPU 开销太大了，**不推荐用于传视频**。

**结论：** 想做高性能图传，请直接研究 `esp_wifi_80211_tx`（发）和 Promiscuous Mode（收）。如果想省事先看效果，用 socket UDP + VLC 播放器。



----------------------------------------------------------------------------------
//   ████████████████████████████████████████████████████████████
//   █  ░░░▓▓▓▒▒▒███▓▓▓░░░  OLDEST HOUSE  ░░░▓▓▓▒▒▒███▓▓▓░░░  █
//   █  🔴  ═══════  ═══════  ═══════  ═══════  ═══════  🔴  █
//   ████████████████████████████████████████████████████████████

完全可以！而且我明确告诉你：**在 PlatformIO (PIO) 中使用 `framework = espidf`，本质上就是在跑原生的 ESP-IDF。**

你提到的这些 `sdkconfig` 优化项，不仅能做，而且是做“高性能透传”的**必选项**。

------

### 1. PIO vs. 原生 ESP-IDF 工具链

在 PIO 里，当你按下“编译”时，它背后调用的就是 `CMake` 和 `ninja`，这和你在终端敲 `idf.py build` 是一模一样的。

- **修改方式**：在 PIO 项目的终端输入 `pio run -t menuconfig`。
- **结果**：它会生成/更新项目根目录下的 `sdkconfig` 文件。这个文件会被编译进你的固件。
- **注意**：如果你在 PIO 里用的是 `framework = arduino`，那么这些设置**无效**（因为库是预编译好的）；但只要你切到了 **`espidf`**，你就是这台机器的绝对主宰。

------

### 2. 深度解析：为什么要勾选这些 IRAM 优化？

在 ESP32 中，内存分为 **Internal RAM (SRAM)** 和 **External Flash**。

- **默认情况**：大部分代码（包括 LwIP 和 WiFi 驱动）都存放在 External Flash 中。CPU 执行时，需要通过 **Cache** 去读取。如果此时 WiFi 正在高速收发，总线压力巨大，Cache 没命中，CPU 就会卡在“等待指令读取”上。
- **IRAM 优化后**：把最核心、最频繁调用的函数（如中断处理、TCP 状态机、校验和计算）直接塞进 CPU 旁边的 **Internal RAM**。

**结论：** 勾选这些选项后，你的 TCP/IP 协议栈处理速度会快 **20%~30%**，且大幅降低了因为 Flash 读取延迟导致的包处理超时（丢包）。

------

### 3. 你提到的优化项对照表 (PIO / ESP-IDF 均适用)

这些选项在 `menuconfig` 中的具体位置（针对最新版 IDF）：

| **优化目标**          | **Menuconfig 路径**                                          | **建议操作**                                     |
| --------------------- | ------------------------------------------------------------ | ------------------------------------------------ |
| **LwIP 核心 IRAM 化** | `Component config` -> `LWIP` -> `Enable LWIP IRAM optimization` | **勾选** (大幅减少总线竞争)                      |
| **TCP 专项优化**      | `Component config` -> `LWIP` -> `Enable IRAM optimization for TCP` | **勾选** (如果你用 TCP 透传，这是关键)           |
| **WiFi 驱动优化**     | `Component config` -> `Wi-Fi` -> `WiFi IRAM optimization`    | **勾选**                                         |
| **WiFi 速度优先级**   | `Component config` -> `Wi-Fi` -> `WiFi Speed optimization`   | 设为 **Optimize for speed** (编译器会开启 `-O2`) |
| **二进制代码大小**    | `Compiler options` -> `Optimization Level`                   | 设为 **Optimize for performance (-O2)**          |

------

### 4. 需要注意的“坑”

虽然开启 IRAM 优化爽如闪电，但有两个代价：

1. **内存占用（SRAM）**：IRAM 优化会消耗宝贵的 **SRAM（内存）**。ESP32 的 IRAM 大约只有 128KB~192KB 左右。如果你勾选了太多优化选项，可能会在编译时报错：`IRAM_0 segment data does not fit`（内存爆了）。
2. **二进制体积**：对你做透传来说，Flash 空间通常很大，这不重要。

------

### 5. 终极建议：如何验证效果？

既然你在做透传适配，我建议你在 PIO 的 `sdkconfig` 中再开启一个隐藏技能：**零拷贝（Zero Copy）**。

在 `Component config -> LWIP` 里找：

- **`CONFIG_LWIP_TCP_MSL`**：减小它（例如 60000ms -> 10000ms），可以更快回收断开的 TCP 连接，防止内存泄漏。
- **`CONFIG_LWIP_MAX_ACTIVE_TCP`**：如果你要做多路透传，调大这个。

#### 你的下一步：

既然你选择了 PIO + ESP-IDF 这一对硬核组合，**你会发现 40Mbps 不再是梦。**

**你想让我演示一下如何在 PIO 的 `platformio.ini` 里通过 `build_flags` 直接“硬编码”这些配置，省得去翻 menuconfig 吗？**
















