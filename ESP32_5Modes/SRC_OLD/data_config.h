#pragma once
#define DATA_CONFIG_H
#include <Arduino.h>
// #include <HardwareSerial.h>



#define _FACTORY_DEBUG
#define FCT_BAUD_DBG 115200
#define DFT_BAUD 115200


// 🐦🐦配置区块🐦🐦
//串口引脚都用默认（FACTORY_DEBUG串口序号和引脚都必须用默认，因为Verbose等系统日志强制发默认序号和引脚）
//ESP32-C3建议开启USB-CDC，会绑定UART0到USB口。这样其他俩对引脚就自由了。

#define FOR_ESP32_32E
// #define FOR_ESP32_C3
// #define FOR_ESP32_S3 //基本也用不到
//无需 define ESP8266, 因为用的不同库

#ifdef FOR_ESP32_32E
constexpr uint8_t AirReadyIndicator_PIN = 33;  // 连接状态
constexpr uint8_t StateIndicator_PIN = 2;             //指示灯
constexpr uint8_t CFG_PIN = 32;            //进入网页调参，自锁按钮
constexpr uint8_t NUM_DFT_DAT_SER = 2;
constexpr uint8_t NUM_DFT_DBG_SER = 0;
// HardwareSerial* dft_dat_ser = &Serial2; //不能用 HardwareSerial* ，因为会重复定义
#define dft_dat_ser &Serial2  // 高版本C++ 17可以用 inline
#define dft_dbg_ser &Serial   // 更安全写法是extern 然后在.cpp里定义
#endif

#ifdef FOR_ESP32_C3
constexpr uint8_t CnctIndicator_PIN = ;
constexpr uint8_t CFG_PIN = ;
constexpr uint8_t LED_PIN = ;
constexpr uint8_t NUM_DFT_DAT_SER = 1;
constexpr uint8_t NUM_DFT_DBG_SER = 0;
// HardwareSerial* dft_dat_ser = &Serial2;
#define dft_dat_ser &Serial1
#define dft_dbg_ser &Serial
#endif

//按钮事件
extern uint8_t modeConnected;
extern bool lastCfgPin;
extern bool web_config_mode;
extern RTC_DATA_ATTR uint8_t TEST_MODE;



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
  HardwareSerial* Serial_data;  //连接外接设备的串口。若在电脑上用python无线调试，则可和debug为同一串口
  uint32_t baud_data;
  
  uint8_t Serial_debug_num;
  HardwareSerial* Serial_debug;  //接usb ttl模块的debug用串口
  uint32_t baud_debug;
  bool DebugON;
};

// 🌧️🌧️Web配置结构体🌧️🌧️
struct ApSsidPwd {
  String SSID;
  String PassWord;
};
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

#define DEBUG_PRINT(x) \
do { \
  if (DEBUG_ON && S_DEBUG) \
  S_DEBUG->print(x); \
} while (0)

#define DEBUG_PRINTLN(x) \
do { \
  if (DEBUG_ON && S_DEBUG) \
  S_DEBUG->println(x); \
} while (0)

#define DEBUG_PRINT_ARRAY_EXT(ptr, len, fmt, sep) \
do { \
  if (DEBUG_ON && S_DEBUG) \
  for (size_t i = 0; i < (len); i++) { \
    if ((fmt) == HEX && (ptr)[i] < 0x10) S_DEBUG->print("0"); \
    S_DEBUG->print((ptr)[i], (fmt)); \
    if (i < (len) - 1) S_DEBUG->print(sep); \
  } \
} while (0)

#define DEBUG_PRINTF(fmt, ...) \
do { \
  if (DEBUG_ON && S_DEBUG){ \
  char temp[128]; \
  snprintf(temp, sizeof(temp), fmt, ##__VA_ARGS__); \
  S_DEBUG->print(temp);} \
} while (0)





// 工厂调试宏 - 编译时决定
#ifdef _FACTORY_DEBUG
#define _FACTORY_DEBUG_PRINT(x) \
do { \
  delay(20); \
  Serial.print(x); \
  delay(20); \
} while (0)

#define _FACTORY_DEBUG_PRINTLN(x) \
do { \
  delay(20); \
  Serial.println(x); \
  delay(20); \
} while (0)

//如果我要Serial.print(x, HEX)怎么办？用下面这个宏
#define _FACTORY_DEBUG_PRINT_ARRAY_EXT(ptr, len, fmt, sep) \
do { \
  delay(20); \
  for (size_t i = 0; i < (len); i++) { \
    if ((fmt) == HEX && (ptr)[i] < 0x10) Serial.print("0"); \
    Serial.print((ptr)[i], (fmt)); \
    if (i < (len) - 1) Serial.print(sep); \
  } \
  Serial.println(); \
  delay(20); \
} while (0)

#define _FACTORY_PRINTF(fmt, ...) \
do { \
  delay(100); \
  char buf[128]; \
  snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
  Serial.print(buf); \
  delay(100); \
} while (0)

#else
// 非工厂模式，这些宏为空
#define _FACTORY_DEBUG_PRINT(x)
#define _FACTORY_DEBUG_PRINTLN(x)
#define _FACTORY_DEBUG_PRINT_ARRAY_EXT(ptr, len, fmt, sep)
#define _FACTORY_PRINTF(fmt, ...)
#endif




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