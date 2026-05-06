#pragma once
#define DATA_CONFIG_H
#include <Arduino.h>
// #include <HardwareSerial.h>
#include "driver/uart.h" // 必须包含底层驱动头文件


// #define _FACTORY_DEBUG
#define FCT_BAUD_DBG 115200
#define DFT_BAUD 115200


// 🐦🐦配置区块🐦🐦
//串口引脚都用默认（FACTORY_DEBUG串口序号和引脚都必须用默认，因为Verbose等系统日志强制发默认序号和引脚）
//ESP32-C3建议开启USB-CDC，会绑定UART0到USB口。这样其他俩对引脚就自由了。

// #define FOR_ESP32_32E
#define FOR_ESP32_C3
// #define FOR_ESP32_S3 //基本也用不到
//无需 define ESP8266, 因为用的不同库

#ifdef FOR_ESP32_32E
constexpr uint8_t AirReadyIndicator_PIN = 33;  // 连接状态
constexpr uint8_t StateIndicator_PIN = 2;             //指示灯
constexpr uint8_t CFG_PIN = 13;            //进入网页调参，自锁按钮 //13就在GND旁，容易焊开关，
constexpr uint8_t NUM_DFT_DAT_SER = 2;
constexpr uint8_t NUM_DFT_DBG_SER = 0;
#define dft_dat_ser &Serial2  // 高版本C++ 17可以用 inline
#define dft_dbg_ser &Serial   // 更安全写法是extern 然后在.cpp里定义
#endif

#ifdef FOR_ESP32_C3
// ┌─────────────────────────────────────────────────────────────────────┐
// │              ESP32-C3 SuperMini 引脚全览 (仅暴露引脚)                │
// │                                                                     │
// │  GPIO0   ADC1，可随意用                                             │
// │  GPIO1   ADC1，可随意用                                             │
// │  GPIO2 ⚠️ STRAPPING PIN：浮空/高=SPI正常启动，不要在此脚外接下拉   │
// │  GPIO3   ADC1，普通IO，推荐做按键                                   │
// │  GPIO4   ADC1 + JTAG，可随意用（无JTAG调试器时当普通IO）           │
// │  GPIO5   JTAG，可随意用                                             │
// │  GPIO6   JTAG，可随意用                                             │
// │  GPIO7   JTAG，可随意用                                             │
// │  GPIO8 ⚠️ STRAPPING PIN + 板载蓝色LED（低电平亮，与ESP32相反！）   │
// │              启动时若被外部拉低，会影响ROM log输出（不影响正常启动）│
// │  GPIO9 ⚠️ STRAPPING PIN + BOOT按键 (低电平=进入UART下载模式)       │
// │              ⛔ 绝对不能接自锁开关！自锁低电平会永远无法正常上电！  │
// │  GPIO10  完全普通IO，无特殊功能，首选外接LED                        │
// │  GPIO20  UART0 RX（USB-CDC开启后此口被释放可作普通IO或Serial1）     │
// │  GPIO21  UART0 TX（同上）                                           │
// │                                                                     │
// │  USB-CDC开启后（-D ARDUINO_USB_CDC_ON_BOOT=1）：                    │
// │    Serial  → USB口（接PC调试）    Serial1 → 任意GPIO（接外设）      │
// └─────────────────────────────────────────────────────────────────────┘

constexpr uint8_t StateIndicator_PIN    = 8;   // 板载蓝色LED（低电平亮）注意：极性与ESP32-32E相反！
                                                // 驱动LED时用 digitalWrite(StateIndicator_PIN, LOW) 才是点亮
constexpr uint8_t AirReadyIndicator_PIN = 10;  // 外接LED/连接状态输出，高电平亮，无strapping风险
constexpr uint8_t CFG_PIN              = 3;    // 网页配置触发按键，配置为INPUT_PULLUP，按下=低电平
                                                // ⚠️ 不能用GPIO9！自锁按钮 + GPIO9低电平 = 卡死在下载模式
constexpr uint8_t NUM_DFT_DAT_SER = 1;
constexpr uint8_t NUM_DFT_DBG_SER = 0;
// UART1 数据串口引脚：USB-CDC开启后 GPIO20/21（原UART0硬件引脚）被释放，可重新分配给UART1
// 这样 USB-C 走 CDC调试，GPIO20/21 走 UART1 接外设，互不干扰
constexpr uint8_t DAT_TX_PIN = 21;  // UART1 TX → 接对端设备 RX
constexpr uint8_t DAT_RX_PIN = 20;  // UART1 RX → 接对端设备 TX
#define dft_dat_ser ((Stream*)&Serial1)  // Stream* 兼容 HardwareSerial，UART1接外设
#define dft_dbg_ser ((Stream*)&Serial)   // Stream* 兼容 HWCDC，USB-CDC调试口
#endif

//按钮事件
extern uint8_t modeConnected;
extern bool lastCfgPin;
extern bool web_config_mode;
extern RTC_DATA_ATTR uint8_t TEST_MODE;


// 此处速度其实主要是为了接收端（电脑/手机）优化。UDP模式下100us间隔发送都没事，但是接收端网卡太堵，或主要是上位机程序太慢，所以间隔得长点
#define SET_DFT_CONFIGS \
AirConfig tcpCfg_dft = {1, 64, 2000, 3000};\
AirConfig udpCfg_dft = {1, 32, 1500, 2500};\
AirConfig sppCfg_dft = {1, 32, 2000, 4000};\
AirConfig bleCfg_dft = {1, 32, 2000, 4000};\
AirConfig espnowCfg_dft = {1, 64, 10000, 20000};
// 不能加注释，也不安全，其实还是建议在 AirConfig 构造之后
// inline AirConfig tcpCfg_dft = {1, 12, 1500, 2000}; 


// 🌧️🌧️配置模式枚举🌧️🌧️
enum Mode {  // 选择当前模式
  MODE_TCP = 1,
  MODE_UDP = 2,
  MODE_SPP = 3,
  MODE_BLE = 4,
  MODE_ESPNOW = 5
};

// 🌧️🌧️AirConfig 结构体🌧️🌧️
struct AirConfig {
  uint16_t flush_min_size;       //必须>=此size才会flush，否则下一循环
  uint16_t flush_thres_size;     //本循环中检测到>=此size或time，就flush
  uint32_t flush_min_time_us;    //必须>=此time才会flush，否则下一循环
  uint32_t flush_thres_time_us;  //本循环中检测到>=此size或time，就flush
};

// 🌧️🌧️串口配置结构体🌧️🌧️
struct SerialCfg {  //原本我用SerialConfig这个名字，结果在某些board中居然是个系统宏或者类！？
  uint8_t Serial_data_num;
  Stream* Serial_data;  // ⚠️ Stream* 而非 HardwareSerial*！
                        // ESP32-C3开启USB-CDC后Serial是HWCDC类型，不是HardwareSerial
                        // HWCDC和HardwareSerial都继承自Stream，用基类指针同时兼容两者
                        // 代价：Stream没有begin()，初始化须在setup()里直接调用Serial/Serial1.begin()
  uint32_t baud_data;
  
  uint8_t Serial_debug_num;
  Stream* Serial_debug;  // 同上，兼容 HWCDC(C3 USB-CDC) 和 HardwareSerial(ESP32)
  uint32_t baud_debug;
  bool DebugON;
};

// 🌧️🌧️ESP Wifi配置结构体🌧️🌧️
struct ApSsidPwd {
  String SSID;
  String PassWord;
};
// 🌧️🌧️TCP/UDP配置结构体🌧️🌧️
struct WifiIpPort {
  uint16_t sendPort;
  uint16_t listenPort;
  String staIP;
  bool sendBroad;
  bool listenBroad;
  uint8_t wifiChannel;
  bool wifiChAuto;
};
// 🌧️🌧️家庭WiFi配置结构体🌧️🌧️
struct HomeWifiConfig {
  String SSID;
  String PassWord;
  bool AutoCnctWifi;
};



// 🐦🐦默认配置🐦🐦
extern AirConfig tcpCfg_dft;
extern AirConfig udpCfg_dft;
extern AirConfig sppCfg_dft;
extern AirConfig bleCfg_dft;
extern AirConfig espnowCfg_dft;

extern SerialCfg serial_DFT;

extern ApSsidPwd apWebCfg_fixed;  //AP名称和密码在烧录时固定，不需要后期修改，避免忘了
extern ApSsidPwd apTcp_fixed;
extern ApSsidPwd apUdp_fixed;
extern ApSsidPwd apEspNow_fixed;
extern WifiIpPort wifiIpPort_dft;
extern String NvsReadyChar_fixed;  //烧录时固定为当前数据类型版本号"V_?_?"

//HomeWifiConfig不需要default，因为在web界面的HomeWifi区块没有default按钮，
//只有 Clear。名称和密码清空，自动连接设置为false


// 🌟🌟当前配置🌟🌟
extern Mode crnt_Mode;
extern AirConfig crnt_AirCfg;
extern SerialCfg crnt_SerialCfg;
//不需要AP名称和密码，因为烧录时固定
//不需要NvsReadyChar_fixed，因为固定
extern WifiIpPort crnt_wifiIpPort;
extern HomeWifiConfig crnt_HomeWifiCfg;

// 配对机子的STA MAC
extern uint8_t MAC_PAIRED_STA[6];
// 🐦🐦默认配置🐦🐦


// 🐻🐻当前连接状态🐻🐻
// 获取自身MAC地址
extern uint8_t self_sta_mac[6];
extern uint8_t self_ap_mac[6];
// 当前连接状态，5模式全局共享
extern bool tcpClientConnected;
extern bool deviceConnected;
extern bool oldDeviceConnected;
extern bool pairing;  //给espnow模式配对用






// 🔧 配置更新相关函数 🔧
void checkNvsData();    //检查NVS数据是否合法，然后执行格式化或读取。开机时调用
void readDataConfig();  // 开机，和web页面读取时调用
// void formatDataConfig();  // 如果新芯片，或新固件版本（数据格式不一致），格式化并写入默认配置
bool storeDataConfig();  // web页面点击apply时会触发，往NVS写入当前设定
void debugNvsCheck(); // 只有FACTORY DEBUG模式下才用到，开头检查NVS数据是否存在

//当前配置的 宏
//模式 宏⬇
#define CRNT_MODE crnt_Mode

//AirConfig 宏⬇
// #define FLUSH_MAX_TIME_US 50000  // 50ms 兜底，所有模式共用

#define FLUSH_MIN_SIZE crnt_AirCfg.flush_min_size
#define FLUSH_THRES_SIZE crnt_AirCfg.flush_thres_size
#define FLUSH_MIN_TIME_US crnt_AirCfg.flush_min_time_us
#define FLUSH_THRES_TIME_US crnt_AirCfg.flush_thres_time_us

//STA模式家庭WIFI配置 宏⬇
#define HOME_WIFI_SSID crnt_HomeWifiCfg.SSID
#define HOME_WIFI_PWD crnt_HomeWifiCfg.PassWord
#define HOME_WIFI_AUTO crnt_HomeWifiCfg.AutoCnctWifi

// 端口配置 宏⬇ 
#define WIFI_CHNL crnt_wifiIpPort.wifiChannel      //WIFI信道
#define WIFI_CH_AUTO crnt_wifiIpPort.wifiChAuto    //自动选择最优信道？
#define ESP32_STA_IP crnt_wifiIpPort.staIP         //ESP32 在STA模式下被分配的IP
#define ESP32_UDP_PORT crnt_wifiIpPort.listenPort  // ESP32 自身端口与监听端口
#define PC_UDP_PORT crnt_wifiIpPort.sendPort       // ESP32 发送目标端口，PC监听此口
#define SEND_BROAD crnt_wifiIpPort.sendBroad       //UDP发送广播？
#define LISTEN_BROAD crnt_wifiIpPort.listenBroad   //UD接收广播？

//串口 宏⬇
#define S_DATA crnt_SerialCfg.Serial_data
#define BAUD_DATA crnt_SerialCfg.baud_data
#define NUM_S_DATA crnt_SerialCfg.Serial_data_num
#define S_DEBUG crnt_SerialCfg.Serial_debug
#define BAUD_DEBUG crnt_SerialCfg.baud_debug
#define NUM_S_DEBUG crnt_SerialCfg.Serial_debug_num
#define DEBUG_ON crnt_SerialCfg.DebugON










// #include <stdio.h>

/**
 * ### 🎓 老哥的底層優化筆記：
 * 1. 拋棄 String(x)：String 會在 Heap 申請空間，高頻調試會導致內存碎片。
 * 2. 統一 printf：ESP32 的 printf 直接對接 UART0，比 Serial.print 快且支持格式化。
 * 3. 數組打印優化：利用 %02X 自動補零，省去 if 判斷。
 */


// --- 核心偵錯宏重構 (極致性能版) ---

// 基礎打印：直接格式化輸出，不產生臨時對象
#define DEBUG_PRINTF(fmt, ...) \
    do { \
        if (DEBUG_ON) printf(fmt, ##__VA_ARGS__); \
    } while (0)

// 換行打印
#define DEBUG_PRINTLN(x) \
    do { \
        if (DEBUG_ON) { \
            /* 使用 C++ 類型檢查，如果是字符數組則直接打印，否則轉 String */ \
            printf("%s\n", String(x).c_str()); \
        } \
    } while (0)

// 單純打印 (不換行)
#define DEBUG_PRINT(x) \
    do { \
        if (DEBUG_ON) printf("%s", String(x).c_str()); \
    } while (0)

/**
 * 數組打印：高效率 HEX 輸出
 * @ptr: 數據指針
 * @len: 長度
 * @sep: 分隔符 (如 " " 或 ":")
 */
#define DEBUG_PRINT_ARRAY_HEX(ptr, len, sep) \
    do { \
        if (DEBUG_ON) { \
            for (size_t i = 0; i < (len); i++) { \
                printf("%02X%s", (ptr)[i], (i < (len) - 1) ? sep : ""); \
            } \
            printf("\n"); \
        } \
    } while (0)

// 兼容舊版宏名
#define DEBUG_PRINT_ARRAY_EXT(ptr, len, fmt, sep) \
    do { \
        if (DEBUG_ON) { \
            if ((fmt) == HEX) { \
                DEBUG_PRINT_ARRAY_HEX(ptr, len, sep); \
            } else { \
                for (size_t i = 0; i < (len); i++) { \
                    printf("%u%s", (ptr)[i], (i < (len) - 1) ? sep : ""); \
                } \
                printf("\n"); \
            } \
        } \
    } while (0)

// --- 工廠偵錯宏 (僅用於初始化階段，保持極端穩定性) ---
#ifdef _FACTORY_DEBUG
    // 使用 Serial.flush() 代替長 delay，確保數據物理發出
    #define _FACTORY_DEBUG_PRINT(x)   do { printf("%s", String(x).c_str());  } while (0)
    #define _FACTORY_DEBUG_PRINTLN(x) do { printf("%s\n", String(x).c_str()); /*uart_wait_tx_done(NUM_S_DEBUG, 100 / portTICK_PERIOD_MS);*/ } while (0)
    
    #define _FACTORY_PRINTF(fmt, ...) \
    do { \
            printf(fmt, ##__VA_ARGS__); \
        } while (0)

    #define _FACTORY_DEBUG_PRINT_ARRAY(ptr, len, sep) \
    do { \
            for (size_t i = 0; i < (len); i++) { \
                printf("%02X%s", (ptr)[i], (i < (len) - 1) ? sep : ""); \
            } \
            printf("\n"); \
    } while (0)

// 兼容舊版宏名
    #define _FACTORY_DEBUG_PRINT_ARRAY_EXT(ptr, len, fmt, sep) \
    do { \
            if ((fmt) == HEX) { \
                _FACTORY_DEBUG_PRINT_ARRAY(ptr, len, sep); \
            } else { \
                for (size_t i = 0; i < (len); i++) { \
                    printf("%u%s", (ptr)[i], (i < (len) - 1) ? sep : ""); \
                } \
                printf("\n"); \
            } \
    } while (0)

#else
    #define _FACTORY_DEBUG_PRINT(x)
    #define _FACTORY_DEBUG_PRINTLN(x)
    #define _FACTORY_PRINTF(fmt, ...)
    #define _FACTORY_DEBUG_PRINT_ARRAY(ptr, len, sep)
    #define _FACTORY_DEBUG_PRINT_ARRAY_EXT(ptr, len, fmt, sep)
#endif










// #include <stdio.h>

// // ### 🎓 小科普：`printf` vs `snprintf`
// // - **`printf`**: 它是 C 语言的标准输出函数。在 ESP32 中，它默认已经通过底层驱动绑定到了 **UART0**。
// // 你不需要指定串口，直接调用它就会从 UART0 喷出字符。
// // - **`snprintf`**: 它的作用是“格式化打印到字符串”。它不会直接输出到串口，而是把你想要的格式（比如 `"%d"`）转换
// // 成字符存进一个 `char` 数组里。你之前宏里用的 `snprintf` + `S_DEBUG->print` 是一种安全的做法，防止缓冲区溢出。

// // --- 核心调试宏重构 ---
// // 基础打印：如果是 UART0 则用 printf，否则用 Arduino 的 print
// // 其实已经强制用Serial了，没什么必要区分了，但保留这个机制以防万一
// #define DEBUG_PRINT(x) \
// do { \
//   if (DEBUG_ON && S_DEBUG) { \
//     if (S_DEBUG == &Serial) printf("%s", String(x).c_str()); \
//     else S_DEBUG->print(x); \
//   } \
// } while (0)

// #define DEBUG_PRINTLN(x) \
// do { \
//   if (DEBUG_ON && S_DEBUG) { \
//     if (S_DEBUG == &Serial) printf("%s\n", String(x).c_str()); \
//     else S_DEBUG->println(x); \
//   } \
// } while (0)

// // 数组打印：优化了 HEX 格式的处理
// #define DEBUG_PRINT_ARRAY_EXT(ptr, len, fmt, sep) \
// do { \
//   if (DEBUG_ON && S_DEBUG) { \
//     for (size_t i = 0; i < (len); i++) { \
//       if (S_DEBUG == &Serial) { \
//         if ((fmt) == HEX) printf("%02X", (ptr)[i]); \
//         else printf("%d", (ptr)[i]); \
//         if (i < (len) - 1) printf("%s", sep); \
//       } else { \
//         if ((fmt) == HEX && (ptr)[i] < 0x10) S_DEBUG->print("0"); \
//         S_DEBUG->print((ptr)[i], (fmt)); \
//         if (i < (len) - 1) S_DEBUG->print(sep); \
//       } \
//     } \
//     if (S_DEBUG == &Serial) printf("\n"); \
//     else S_DEBUG->println(); \
//   } \
// } while (0)

// // 格式化打印：直接利用底层 printf 的威力
// #define DEBUG_PRINTF(fmt, ...) \
// do { \
//   if (DEBUG_ON && S_DEBUG) { \
//     if (S_DEBUG == &Serial) printf(fmt, ##__VA_ARGS__); \
//     else { \
//       char temp[128]; \
//       snprintf(temp, sizeof(temp), fmt, ##__VA_ARGS__); \
//       S_DEBUG->print(temp); \
//     } \
//   } \
// } while (0)

// // --- 工厂调试宏 (保持 Serial 兼容性，因为此时驱动还没接管) ---
// #ifdef _FACTORY_DEBUG
// #define _FACTORY_DEBUG_PRINT(x)      do { delay(20); Serial.print(x); delay(20); } while (0)
// #define _FACTORY_DEBUG_PRINTLN(x)    do { delay(20); Serial.println(x); delay(20); } while (0)
// #define _FACTORY_PRINTF(fmt, ...)    do { delay(100); char buf[128]; snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); Serial.print(buf); delay(100); } while (0)
// #else
// #define _FACTORY_DEBUG_PRINT(x)
// #define _FACTORY_DEBUG_PRINTLN(x)
// #define _FACTORY_PRINTF(fmt, ...)
// #endif











// // 基础打印函数
// void debugPrint(const char* message);
// void debugPrintln(const char* message);
// // 支持多种类型
// template<typename T>
// void debugPrint(T value);
// template<typename T>
// void debugPrintln(T value);
// // 格式化输出
// void debugPrintf(const char* format, ...);



// Leave Some Spaces.....