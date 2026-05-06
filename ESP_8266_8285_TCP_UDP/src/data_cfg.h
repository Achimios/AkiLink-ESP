#pragma once
#include <Arduino.h>
#define DATA_CFG_H

// ============================================================
//  ESP8266 / ESP8285 (ESP-01 / ESP-01S)  data_cfg.h
//  Ported from ESP32_Transmission_5Modes_V3 — TCP & UDP only
// ============================================================

// #define _FACTORY_DEBUG        // 出厂调试开关（编译期）
#define FCT_BAUD_DBG 115200   // 工厂调试波特率 (Serial1)
#define DFT_BAUD     115200   // 默认数据串口波特率


// ============================================================
//  🐦🐦 引脚配置区块 🐦🐦
// ============================================================
// ESP-01S 可用GPIO极少:
//   GPIO0  — 开机时必须为HIGH(内部上拉)，否则进入下载模式
//             正常运行后可做通用IO，这里用作 CFG_PIN (自锁按钮，常态断开=HIGH)
//   GPIO2  — 开机时必须为HIGH(内部上拉)
//             ESP-01S板载蓝色LED(低电平点亮)
//             同时也是 Serial1 TX (UART1 TX)
//   GPIO1  — UART0 TX (数据串口)
//   GPIO3  — UART0 RX (数据串口)
//
// ⚠️ GPIO2 冲突: Serial1 TX 和板载LED共用 GPIO2
//    DebugON = true  → GPIO2 用作 Serial1 TX (调试输出)，LED不可用
//    DebugON = false → GPIO2 用作 LED 指示灯
// ============================================================

constexpr uint8_t StateIndicator_PIN = 2;   // 板载LED (GPIO2, 低电平点亮, 与Serial1 TX复用)
constexpr uint8_t CFG_PIN            = 0;   // 进入网页调参 (GPIO0, 自锁按钮，常态HIGH)
// AirReadyIndicator_PIN: ESP-01S没有多余引脚，由LED闪烁模式区分


// ============================================================
//  🐦🐦 默认AirConfig参数 🐦🐦
// ============================================================
// ESP8266 CPU 80/160MHz，处理能力弱于ESP32，时间间隔适当放大
#define SET_DFT_CONFIGS \
AirConfig tcpCfg_dft = {1, 64, 3000, 5000};\
AirConfig udpCfg_dft = {1, 32, 2000, 3500};


#define AP_NAME_WEB "WEB_网址10.0.0.1，密8个1"
#define AP_NAME_TCP "TCP_IP_10.0.0.1，密8个1"
#define AP_NAME_UDP "UDP_IP_10.0.0.1，密8个1"
#define AP_PWD "11111111"  //默认密码，必须8位以上


// ============================================================
//  🌧️🌧️ 配置模式枚举 🌧️🌧️
// ============================================================
enum Mode {
  MODE_TCP = 1,
  MODE_UDP = 2
  // ESP8266 不支持 BLE / SPP
  // ESP-NOW 在8266上不稳定，不提供
};


// ============================================================
//  🌧️🌧️ AirConfig 结构体 🌧️🌧️
// ============================================================
struct AirConfig {
  uint16_t flush_min_size;       // 必须>=此size才会flush，否则下一循环
  uint16_t flush_thres_size;     // 本循环中检测到>=此size或time，就flush
  uint32_t flush_min_time_us;    // 必须>=此time才会flush，否则下一循环
  uint32_t flush_thres_time_us;  // 本循环中检测到>=此size或time，就flush
};


// ============================================================
//  🌧️🌧️ 串口配置结构体 🌧️🌧️
// ============================================================
// ESP8266 串口是固定映射，无需 HardwareSerial* 指针:
//   Serial  (UART0) : TX=GPIO1, RX=GPIO3  — 数据串口
//   Serial1 (UART1) : TX=GPIO2, 无RX      — 调试串口(仅发送)
struct SerialCfg {
  uint32_t baud_data;     // UART0 波特率
  uint32_t baud_debug;    // UART1 波特率
  bool DebugON;           // 运行时调试开关
};


// ============================================================
//  🌧️🌧️ WiFi 配置结构体 🌧️🌧️
// ============================================================

struct WifiIpPort {
  uint16_t sendPort;       // 发送目标端口 (PC/GCS监听此端口)
  uint16_t listenPort;     // ESP自身监听端口
  String   staIP;          // STA模式下被分配的IP
  bool     sendBroad;      // UDP发送广播?
  bool     listenBroad;    // UDP接收所有IP?（不是 是否接收广播）
  uint8_t  wifiChannel;    // WiFi信道 (1-13)
  bool     wifiChAuto;     // 自动选择最优信道?
};

struct HomeWifiConfig {
  String SSID;
  String PassWord;
  bool AutoCnctWifi;
};


// ============================================================
//  🐦🐦 默认配置 extern 声明 🐦🐦
// ============================================================
extern AirConfig tcpCfg_dft;
extern AirConfig udpCfg_dft;

extern SerialCfg serial_DFT;


extern WifiIpPort wifiIpPort_dft;
extern String CfgReadyTag_fixed;   // 烧录时固定，LittleFS数据格式版本校验 "V_?_?"

// HomeWifiConfig不需要default——web界面只有Clear按钮


// ============================================================
//  🌟🌟 当前运行时配置 🌟🌟
// ============================================================
extern Mode crnt_Mode;
extern AirConfig crnt_AirCfg;
extern SerialCfg crnt_SerialCfg;
extern WifiIpPort crnt_wifiIpPort;
extern HomeWifiConfig crnt_HomeWifiCfg;

// 按钮 / Web配置模式
extern bool lastCfgPin;
extern bool web_config_mode;


// ============================================================
//  🐻🐻 当前连接状态 🐻🐻
// ============================================================
extern uint8_t self_sta_mac[6];
extern uint8_t self_ap_mac[6];

extern bool tcpClientConnected;
extern bool deviceConnected;
extern bool oldDeviceConnected;


// ============================================================
//  🔧 配置持久化函数 (LittleFS + ArduinoJson) 🔧
// ============================================================
void checkStoredConfig();   // 开机时调用：校验LittleFS数据，不合法则格式化并写默认值
void readDataConfig();      // 从LittleFS读取配置到crnt_xxx
bool storeDataConfig();     // web页面Apply时写入LittleFS
void debugConfigCheck();    // 仅 _FACTORY_DEBUG 时用，打印当前存储内容


// ============================================================
//  📌 当前配置的便捷宏
// ============================================================

// 模式
#define CRNT_MODE crnt_Mode

// AirConfig
#define FLUSH_MIN_SIZE      crnt_AirCfg.flush_min_size
#define FLUSH_THRES_SIZE    crnt_AirCfg.flush_thres_size
#define FLUSH_MIN_TIME_US   crnt_AirCfg.flush_min_time_us
#define FLUSH_THRES_TIME_US crnt_AirCfg.flush_thres_time_us

// STA模式家庭WiFi
#define HOME_WIFI_SSID crnt_HomeWifiCfg.SSID
#define HOME_WIFI_PWD  crnt_HomeWifiCfg.PassWord
#define HOME_WIFI_AUTO crnt_HomeWifiCfg.AutoCnctWifi

// 端口/信道
#define WIFI_CHNL       crnt_wifiIpPort.wifiChannel   // WiFi信道
#define WIFI_CH_AUTO    crnt_wifiIpPort.wifiChAuto     // 自动选择最优信道?
#define ESP_STA_IP      crnt_wifiIpPort.staIP          // STA模式下被分配的IP
#define ESP_UDP_PORT    crnt_wifiIpPort.listenPort     // ESP自身监听端口
#define PC_UDP_PORT     crnt_wifiIpPort.sendPort       // 发送目标端口
#define SEND_BROAD      crnt_wifiIpPort.sendBroad      // UDP发送广播?
#define LISTEN_BROAD    crnt_wifiIpPort.listenBroad    // UDP接收广播?

// 串口 — ESP8266 固定映射
#define S_DATA      Serial    // UART0: 数据口 (TX=GPIO1, RX=GPIO3)
#define BAUD_DATA   crnt_SerialCfg.baud_data
#define NUM_S_DATA  0         // UART编号

#define S_DEBUG     Serial1   // UART1: 调试口 (TX=GPIO2, 仅TX)
#define BAUD_DEBUG  crnt_SerialCfg.baud_debug
#define NUM_S_DEBUG 1
#define DEBUG_ON    crnt_SerialCfg.DebugON


// ============================================================
//  🎓 调试宏 — ESP8266专用
// ============================================================
// ⚠️ 关键区别: ESP32上 printf() 默认走UART0，而ESP32的UART0是调试口没问题
//    但ESP8266的 UART0 是数据口! 所以绝对不能用裸 printf 做调试输出
//    所有调试输出必须走 Serial1.printf() (UART1, TX=GPIO2)

// --- 运行时调试宏 ---
#define DEBUG_PRINTF(fmt, ...) \
    do { \
        if (DEBUG_ON) S_DEBUG.printf(fmt, ##__VA_ARGS__); \
    } while (0)

#define DEBUG_PRINTLN(x) \
    do { \
        if (DEBUG_ON) { S_DEBUG.print(x); S_DEBUG.print('\n'); } \
    } while (0)

#define DEBUG_PRINT(x) \
    do { \
        if (DEBUG_ON) S_DEBUG.print(x); \
    } while (0)

#define DEBUG_PRINT_ARRAY_HEX(ptr, len, sep) \
    do { \
        if (DEBUG_ON) { \
            for (size_t i = 0; i < (len); i++) { \
                S_DEBUG.printf("%02X%s", (ptr)[i], (i < (len) - 1) ? sep : ""); \
            } \
            S_DEBUG.print('\n'); \
        } \
    } while (0)

#define DEBUG_PRINT_ARRAY_EXT(ptr, len, fmt, sep) \
    do { \
        if (DEBUG_ON) { \
            if ((fmt) == HEX) { \
                DEBUG_PRINT_ARRAY_HEX(ptr, len, sep); \
            } else { \
                for (size_t i = 0; i < (len); i++) { \
                    S_DEBUG.printf("%u%s", (ptr)[i], (i < (len) - 1) ? sep : ""); \
                } \
                S_DEBUG.print('\n'); \
            } \
        } \
    } while (0)


// --- 工厂调试宏 (仅编译期 _FACTORY_DEBUG 启用) ---
#ifdef _FACTORY_DEBUG
    #define _FACTORY_DEBUG_PRINT(x)   do { S_DEBUG.print(x);  } while (0)
    #define _FACTORY_DEBUG_PRINTLN(x) do { S_DEBUG.print(x); S_DEBUG.print('\n'); S_DEBUG.flush(); } while (0)

    #define _FACTORY_PRINTF(fmt, ...) \
        do { S_DEBUG.printf(fmt, ##__VA_ARGS__); } while (0)

    #define _FACTORY_DEBUG_PRINT_ARRAY(ptr, len, sep) \
        do { \
            for (size_t i = 0; i < (len); i++) { \
                S_DEBUG.printf("%02X%s", (ptr)[i], (i < (len) - 1) ? sep : ""); \
            } \
            S_DEBUG.print('\n'); \
        } while (0)

    #define _FACTORY_DEBUG_PRINT_ARRAY_EXT(ptr, len, fmt, sep) \
        do { \
            if ((fmt) == HEX) { \
                _FACTORY_DEBUG_PRINT_ARRAY(ptr, len, sep); \
            } else { \
                for (size_t i = 0; i < (len); i++) { \
                    S_DEBUG.printf("%u%s", (ptr)[i], (i < (len) - 1) ? sep : ""); \
                } \
                S_DEBUG.print('\n'); \
            } \
        } while (0)
#else
    #define _FACTORY_DEBUG_PRINT(x)
    #define _FACTORY_DEBUG_PRINTLN(x)
    #define _FACTORY_PRINTF(fmt, ...)
    #define _FACTORY_DEBUG_PRINT_ARRAY(ptr, len, sep)
    #define _FACTORY_DEBUG_PRINT_ARRAY_EXT(ptr, len, fmt, sep)
#endif


// Leave Some Spaces.....
