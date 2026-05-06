// ESP32 NVS的键名长度限制是15个字符!!!
// flush_min_time_us = 17个字符 ❌ 超标！
// flush_thres_time_us = 19个字符 ❌ 严重超标！

// 📊 NVS类型映射表
// 你的变量类型	NVS函数	说明
// uint8_t	        getUChar() / putUChar()	    8位无符号整数
// int8_t	        getChar() / putChar()	      8位有符号整数
// uint16_t	      getUShort() / putUShort()	  16位无符号整数
// int16_t	        getShort() / putShort()   	16位有符号整数
// uint32_t	      getUInt() / putUInt()	      32位无符号整数
// int32_t	        getInt() / putInt()	        32位有符号整数
// unsigned long	  getUInt() / putUInt()	      等同 uint32_t
// long	          getInt() / putInt()	        等同 int32_t
// bool	          getBool() / putBool()	      布尔值
// String	        getString() / putString()	  字符串
// float	          getFloat() / putFloat()	    浮点数
// double	        getDouble() / putDouble()	  双精度
// Blob            getBytes() / putBytes()	    二进制数据块


#include "data_config.h"

#include <Preferences.h>
#include <stdarg.h>
#include <stdio.h>

// 按钮事件
uint8_t modeConnected = 0;
bool lastCfgPin = false;
bool web_config_mode = false;
RTC_DATA_ATTR uint8_t TEST_MODE = 0;

Preferences preferences;  // 用于操作NVS

// 🐦🐦默认配置🐦🐦   🐦🐦   🐦🐦   🐦🐦   🐦🐦
//              Default值单纯是给新芯片或新版本初始化用的，执行存入NVS后就没意义了，
//              每次开机后实际以NVS数据为准，除非NVS数据损坏或版本不匹配需要格式化。
String NvsReadyChar_fixed = "V_1_7";  // 数据版本号

SET_DFT_CONFIGS

#ifdef _FACTORY_DEBUG
SerialCfg serial_DFT = {NUM_DFT_DBG_SER, dft_dbg_ser, FCT_BAUD_DBG, NUM_DFT_DBG_SER, dft_dbg_ser, FCT_BAUD_DBG, true};
#else
SerialCfg serial_DFT = {NUM_DFT_DAT_SER, dft_dat_ser, DFT_BAUD, NUM_DFT_DBG_SER, dft_dbg_ser, DFT_BAUD, false};
#endif

WifiIpPort wifiIpPort_dft = {14550, 5761, "192.168.x.x", true, true, 13, false};

// 🌟🌟当前配置🌟🌟   🌟🌟   🌟🌟   🌟🌟   🌟🌟
Mode crnt_Mode = MODE_TCP;              // 添加默认值
AirConfig crnt_AirCfg = tcpCfg_dft;     // 初始化为默认值
SerialCfg crnt_SerialCfg = serial_DFT;  // 初始化为默认值
WifiIpPort crnt_wifiIpPort = wifiIpPort_dft;
HomeWifiConfig crnt_HomeWifiCfg = {"我家Wifi", "password", false};

uint8_t MAC_PAIRED_STA[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};  // 默认全0，不能为全FF，对于ESPNOW，非法地址都是广播
// 记录对方STA地址用于通讯。而对方AP地址用于配对，不参与通讯。AP地址第六字节为STA地址的第六字节加1（如果是FF则回绕为00）。
// 🐦🐦默认配置🐦🐦   🐦🐦   🐦🐦   🐦🐦   🐦🐦

// 🐻🐻当前连接状态🐻🐻   🐻🐻   🐻🐻   🐻🐻
uint8_t self_sta_mac[6];  // 初始化自身MAC地址
uint8_t self_ap_mac[6];
bool tcpClientConnected = false;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool pairing = false;


// 🔧 NVS键名定义（必须 ≤15 字符）🔧
// 使用缩写，所有键名长度检查过
#define NVS_KEY_VERSION "ver"  // 版本号

#define NVS_KEY_MODE "mode"          // 模式
#define NVS_KEY_MIN_SIZE "min_sz"    // 最小大小
#define NVS_KEY_THRES_SIZE "thr_sz"  // 阈值大小
#define NVS_KEY_MIN_TIME "min_tm"    // 最小时间
#define NVS_KEY_THRES_TIME "thr_tm"  // 阈值时间

#define NVS_KEY_SRL_DATA_NUM "srl_dt_num"    // 数据串口号
#define NVS_KEY_SRL_DEBUG_NUM "srl_dbg_num"  // 调试串口号
#define NVS_KEY_BAUD_DATA "baud_dt"          // 数据波特率
#define NVS_KEY_BAUD_DEBUG "baud_dbg"        // 调试波特率
#define NVS_KEY_DEBUG_ON "dbg_on"            // 调试开关?

#define NVS_KEY_WIFI_SSID "wifi_ssid"  // WiFi SSID
#define NVS_KEY_WIFI_PASS "wifi_pass"  // WiFi密码
#define NVS_KEY_WIFI_AUTO "wifi_auto"  // WiFi自动连接?

#define NVS_KEY_SND_PORT "sndPort"     // 发送端口
#define NVS_KEY_LSN_PORT "lsnPort"     // 监听端口
#define NVS_KEY_STA_IP "staIP"         // STA IP地址
#define NVS_KEY_SND_BRD "sndBrd"       // 发送广播?
#define NVS_KEY_LSN_BRD "lsnBrd"       // 监听广播?
#define NVS_KEY_WIFI_CH "wifiCh"       // WiFi信道
#define NVS_KEY_WIFI_CH_AT "wifiChAt"  // WiFi信道自动选择?

#define NVS_KEY_MAC_PAIRED_STA "macPrdSta"  // ESP-NOW配对的MAC地址，6字节二进制数据



// 🔧 配置更新相关函数 🔧     🔧      🔧     🔧     🔧     🔧     🔧     🔧     🔧     🔧
void checkNvsData() {  //    ✅️    ✅️    ✅️    ✅️    ✅️    ✅️    ✅️
  _FACTORY_DEBUG_PRINTLN("[NVS] 检查NVS数据...");
  String magic = "";  // 先初始化
  if (preferences.begin("config", true)) {
    magic = preferences.getString(NVS_KEY_VERSION, "");
    preferences.end();
  } else {
    _FACTORY_DEBUG_PRINTLN("[NVS] 错误：无法打开NVS");
    magic = "";  // 确保为空
  }

  _FACTORY_DEBUG_PRINT("[NVS] 版本号: ");
  _FACTORY_DEBUG_PRINTLN(magic);

  // 检查是否需要格式化
  bool needFormat = false;
  if (magic.length() == 0) {
    _FACTORY_DEBUG_PRINTLN("[NVS] 首次使用或数据损坏，格式化...");
    needFormat = true;
  } else if (magic != NvsReadyChar_fixed) {
    _FACTORY_DEBUG_PRINTLN("[NVS] 版本不匹配");
    needFormat = true;
  } else {
    _FACTORY_DEBUG_PRINTLN("[NVS] 版本匹配");
  }

  if (needFormat) {
    _FACTORY_DEBUG_PRINTLN("[NVS] 格式化配置...");
    // formatDataConfig();
    storeDataConfig();  // 此时crnt就是dft
  }
  delay(1000);
  readDataConfig();
}

#define _BackUp_V

#ifdef _BackUp_V
void readDataConfig() {  //      👁️    👁️    👁️    👁️    👁️    👁️    👁️
  _FACTORY_DEBUG_PRINTLN("[NVS] 读取配置...");

  if (!preferences.begin("config", true)) {
    _FACTORY_DEBUG_PRINTLN("[NVS] 错误：无法打开NVS");
    return;
  }

  // 📌 读取模式 - uint8_t
  uint8_t mode_val = preferences.getUChar(NVS_KEY_MODE, MODE_BLE);
  CRNT_MODE = static_cast<Mode>(mode_val);
  if (CRNT_MODE < MODE_TCP || CRNT_MODE > MODE_ESPNOW) {
    CRNT_MODE = MODE_BLE;  // 安全回退
  }
  _FACTORY_DEBUG_PRINT("[NVS] 模式: ");
  _FACTORY_DEBUG_PRINTLN(CRNT_MODE);

  // 📌 读取Air配置 - 注意类型匹配！
  // flush_min_size 是 uint16_t，用 getUShort
  FLUSH_MIN_SIZE = preferences.getUShort(NVS_KEY_MIN_SIZE, FLUSH_MIN_SIZE);
  FLUSH_THRES_SIZE = preferences.getUShort(NVS_KEY_THRES_SIZE, FLUSH_THRES_SIZE);

  // flush_min_time_us 是 uint32_t，用 getUInt
  FLUSH_MIN_TIME_US = preferences.getUInt(NVS_KEY_MIN_TIME, FLUSH_MIN_TIME_US);
  FLUSH_THRES_TIME_US = preferences.getUInt(NVS_KEY_THRES_TIME, FLUSH_THRES_TIME_US);

  _FACTORY_DEBUG_PRINT("[NVS] Air配置: ");
  _FACTORY_DEBUG_PRINT(FLUSH_MIN_SIZE);
  _FACTORY_DEBUG_PRINT("~");
  _FACTORY_DEBUG_PRINT(FLUSH_THRES_SIZE);
  _FACTORY_DEBUG_PRINT("字节, ");
  _FACTORY_DEBUG_PRINT(FLUSH_MIN_TIME_US);
  _FACTORY_DEBUG_PRINT("~");
  _FACTORY_DEBUG_PRINT(FLUSH_THRES_TIME_US);
  _FACTORY_DEBUG_PRINTLN("μs");

  // 📌 读取串口配置
  // Serial_data_num 是 uint8_t，用 getUChar
  NUM_S_DATA = preferences.getUChar(NVS_KEY_SRL_DATA_NUM, NUM_S_DATA);
  NUM_S_DEBUG = preferences.getUChar(NVS_KEY_SRL_DEBUG_NUM, NUM_S_DEBUG);

  // 设置串口指针
  switch (NUM_S_DATA) {
    case 0: S_DATA = (Stream*)&Serial; break;
    case 1: S_DATA = (Stream*)&Serial1; break;
    default:
      S_DATA = S_DATA;  // 双重保险
      NUM_S_DATA = NUM_S_DATA;
      break;
  }

  switch (NUM_S_DEBUG) {
    case 0: S_DEBUG = (Stream*)&Serial; break;
    // case 1: S_DEBUG = (Stream*)&Serial1; break;
    default:
      S_DEBUG = S_DEBUG;  // 双重保险
      NUM_S_DEBUG = NUM_S_DEBUG;
      break;
  }

  // 波特率是 uint32_t，用 getUInt
  BAUD_DATA = preferences.getUInt(NVS_KEY_BAUD_DATA, BAUD_DATA);
  BAUD_DEBUG = preferences.getUInt(NVS_KEY_BAUD_DEBUG, BAUD_DEBUG);
  DEBUG_ON = preferences.getBool(NVS_KEY_DEBUG_ON, DEBUG_ON);

  _FACTORY_DEBUG_PRINT("[NVS] 串口配置: 数据=UART");
  _FACTORY_DEBUG_PRINT((int)NUM_S_DATA);
  _FACTORY_DEBUG_PRINT(" @ ");
  _FACTORY_DEBUG_PRINT(BAUD_DATA);
  _FACTORY_DEBUG_PRINT(" baud, 调试=UART");
  _FACTORY_DEBUG_PRINT((int)NUM_S_DEBUG);
  _FACTORY_DEBUG_PRINT(" @ ");
  _FACTORY_DEBUG_PRINT(BAUD_DEBUG);
  _FACTORY_DEBUG_PRINT(" baud, 调试");
  _FACTORY_DEBUG_PRINTLN(DEBUG_ON ? "开启" : "关闭");

  // 📌 读取WiFi配置
  HOME_WIFI_SSID = preferences.getString(NVS_KEY_WIFI_SSID, "");
  HOME_WIFI_PWD = preferences.getString(NVS_KEY_WIFI_PASS, "");
  HOME_WIFI_AUTO = preferences.getBool(NVS_KEY_WIFI_AUTO, false);
  PC_UDP_PORT = preferences.getUShort(NVS_KEY_SND_PORT, PC_UDP_PORT);
  ESP32_UDP_PORT = preferences.getUShort(NVS_KEY_LSN_PORT, ESP32_UDP_PORT);
  ESP32_STA_IP = preferences.getString(NVS_KEY_STA_IP, ESP32_STA_IP);
  SEND_BROAD = preferences.getBool(NVS_KEY_SND_BRD, SEND_BROAD);
  LISTEN_BROAD = preferences.getBool(NVS_KEY_LSN_BRD, LISTEN_BROAD);
  WIFI_CHNL = preferences.getUChar(NVS_KEY_WIFI_CH, WIFI_CHNL);
  WIFI_CH_AUTO = preferences.getBool(NVS_KEY_WIFI_CH_AT, WIFI_CH_AUTO);

  preferences.getBytes(NVS_KEY_MAC_PAIRED_STA, MAC_PAIRED_STA, 6);

  _FACTORY_DEBUG_PRINT("[NVS] WiFi配置: SSID=【");
  _FACTORY_DEBUG_PRINT(HOME_WIFI_SSID);
  _FACTORY_DEBUG_PRINT("】密码=【");
  _FACTORY_DEBUG_PRINT(HOME_WIFI_PWD);
  _FACTORY_DEBUG_PRINT("】, 自动连接=");
  _FACTORY_DEBUG_PRINT(HOME_WIFI_AUTO ? "  是" : "否");
  _FACTORY_DEBUG_PRINT("  信道=");
  _FACTORY_DEBUG_PRINT(WIFI_CHNL);
  _FACTORY_DEBUG_PRINT("  自动择优=");
  _FACTORY_DEBUG_PRINTLN(WIFI_CH_AUTO ? "  是" : "否");
  _FACTORY_DEBUG_PRINT("  发送端口=");
  _FACTORY_DEBUG_PRINT(PC_UDP_PORT);
  _FACTORY_DEBUG_PRINT(SEND_BROAD ? "  广播" : "单播");
  _FACTORY_DEBUG_PRINT(", 接收端口=");
  _FACTORY_DEBUG_PRINT(ESP32_UDP_PORT);
  _FACTORY_DEBUG_PRINT(LISTEN_BROAD ? "  所有设备" : "配对设备");
  _FACTORY_DEBUG_PRINT(", staIP=");
  _FACTORY_DEBUG_PRINT(ESP32_STA_IP);
  _FACTORY_DEBUG_PRINT(", 配对MAC=");
  _FACTORY_DEBUG_PRINT_ARRAY_EXT(MAC_PAIRED_STA, 6, HEX, ":");
  _FACTORY_DEBUG_PRINTLN("");

  preferences.end();

  _FACTORY_DEBUG_PRINTLN("[NVS] 配置读取完成");
}
#endif

// void formatDataConfig() {  //     🏠     🏠 🏠      🏠 🏠🏠     🏠  🏠    🏠     🏠   🏠   🏠
//   _FACTORY_DEBUG_PRINTLN("[NVS] 格式化配置...");
//   if (!preferences.begin("config", false)) {
//     _FACTORY_DEBUG_PRINTLN("[NVS] 错误：无法打开NVS进行格式化");
//     return;
//   }
//   // 清除所有旧数据
//   if (!preferences.clear()) {
//     _FACTORY_DEBUG_PRINTLN("[NVS] 警告：清除NVS失败");
//   }
//   // 写入默认配置 - 注意类型匹配！
//   preferences.putUChar(NVS_KEY_MODE, MODE_BLE);
//   省略……
//   preferences.end();
//   _FACTORY_DEBUG_PRINTLN("[NVS] 格式化完成");
//   // 立即应用默认配置到当前变量
//   CRNT_MODE = MODE_BLE;
//   省略……
// }

bool storeDataConfig() {  //     ⬆️     ⬆️     ⬆️     ⬆️     ⬆️     ⬆️     ⬆️     ⬆️
  _FACTORY_DEBUG_PRINTLN("[NVS] 保存配置到NVS...");

  if (!preferences.begin("config", false)) {
    _FACTORY_DEBUG_PRINTLN("[NVS] 错误：无法打开NVS存储");
    return true;
  }

  // 写入版本号
  preferences.putString(NVS_KEY_VERSION, NvsReadyChar_fixed);
  _FACTORY_DEBUG_PRINT("[NVS] 版本号: ");
  _FACTORY_DEBUG_PRINTLN(NvsReadyChar_fixed);


  // 写入当前配置 - 注意类型匹配！
  preferences.putUChar(NVS_KEY_MODE, CRNT_MODE);

  preferences.putUShort(NVS_KEY_MIN_SIZE, FLUSH_MIN_SIZE);
  preferences.putUShort(NVS_KEY_THRES_SIZE, FLUSH_THRES_SIZE);
  preferences.putUInt(NVS_KEY_MIN_TIME, FLUSH_MIN_TIME_US);
  preferences.putUInt(NVS_KEY_THRES_TIME, FLUSH_THRES_TIME_US);

  _FACTORY_DEBUG_PRINT("[NVS] 保存模式: ");
  _FACTORY_DEBUG_PRINTLN(CRNT_MODE);
  _FACTORY_DEBUG_PRINT("[NVS] 保存Air配置: ");
  _FACTORY_DEBUG_PRINT(FLUSH_MIN_SIZE);
  _FACTORY_DEBUG_PRINT("/");
  _FACTORY_DEBUG_PRINT(FLUSH_THRES_SIZE);
  _FACTORY_DEBUG_PRINT("字节, ");
  _FACTORY_DEBUG_PRINT(FLUSH_MIN_TIME_US);
  _FACTORY_DEBUG_PRINT("/");
  _FACTORY_DEBUG_PRINT(FLUSH_THRES_TIME_US);
  _FACTORY_DEBUG_PRINTLN("μs");

  preferences.putUChar(NVS_KEY_SRL_DATA_NUM, NUM_S_DATA);
  preferences.putUChar(NVS_KEY_SRL_DEBUG_NUM, NUM_S_DEBUG);
  preferences.putUInt(NVS_KEY_BAUD_DATA, BAUD_DATA);
  preferences.putUInt(NVS_KEY_BAUD_DEBUG, BAUD_DEBUG);
  preferences.putBool(NVS_KEY_DEBUG_ON, DEBUG_ON);

  _FACTORY_DEBUG_PRINT("[NVS] 保存串口配置: 数据=UART");
  _FACTORY_DEBUG_PRINT(NUM_S_DATA);
  _FACTORY_DEBUG_PRINT(" @ ");
  _FACTORY_DEBUG_PRINT(BAUD_DATA);
  _FACTORY_DEBUG_PRINT(" baud, 调试=UART");
  _FACTORY_DEBUG_PRINT(NUM_S_DEBUG);
  _FACTORY_DEBUG_PRINT(" @ ");
  _FACTORY_DEBUG_PRINT(BAUD_DEBUG);
  _FACTORY_DEBUG_PRINT(" baud, 调试");
  _FACTORY_DEBUG_PRINTLN(DEBUG_ON ? "开启" : "关闭");

  preferences.putString(NVS_KEY_WIFI_SSID, HOME_WIFI_SSID);
  preferences.putString(NVS_KEY_WIFI_PASS, HOME_WIFI_PWD);
  preferences.putBool(NVS_KEY_WIFI_AUTO, HOME_WIFI_AUTO);
  preferences.putUShort(NVS_KEY_SND_PORT, PC_UDP_PORT);
  preferences.putUShort(NVS_KEY_LSN_PORT, ESP32_UDP_PORT);
  preferences.putString(NVS_KEY_STA_IP, ESP32_STA_IP);
  preferences.putBool(NVS_KEY_SND_BRD, SEND_BROAD);
  preferences.putBool(NVS_KEY_LSN_BRD, LISTEN_BROAD);
  preferences.putUChar(NVS_KEY_WIFI_CH, WIFI_CHNL);
  preferences.putBool(NVS_KEY_WIFI_CH_AT, WIFI_CH_AUTO);
  preferences.putBytes(NVS_KEY_MAC_PAIRED_STA, MAC_PAIRED_STA, 6);

  _FACTORY_DEBUG_PRINT("[NVS] WiFi配置: SSID=【");
  _FACTORY_DEBUG_PRINT(HOME_WIFI_SSID);
  _FACTORY_DEBUG_PRINT("】密码=【");
  _FACTORY_DEBUG_PRINT(HOME_WIFI_PWD);
  _FACTORY_DEBUG_PRINT("】, 自动连接=");
  _FACTORY_DEBUG_PRINT(HOME_WIFI_AUTO ? "  是" : "否");
  _FACTORY_DEBUG_PRINT("  信道=");
  _FACTORY_DEBUG_PRINT(WIFI_CHNL);
  _FACTORY_DEBUG_PRINT("  自动择优=");
  _FACTORY_DEBUG_PRINTLN(WIFI_CH_AUTO ? "  是" : "否");
  _FACTORY_DEBUG_PRINT("  发送端口=");
  _FACTORY_DEBUG_PRINT(PC_UDP_PORT);
  _FACTORY_DEBUG_PRINT(SEND_BROAD ? "  广播" : "单播");
  _FACTORY_DEBUG_PRINT(", 接收端口=");
  _FACTORY_DEBUG_PRINT(ESP32_UDP_PORT);
  _FACTORY_DEBUG_PRINT(LISTEN_BROAD ? "  所有设备" : "配对设备");
  _FACTORY_DEBUG_PRINT(", staIP=");
  _FACTORY_DEBUG_PRINT(ESP32_STA_IP);
  _FACTORY_DEBUG_PRINT(", 配对MAC=");
  _FACTORY_DEBUG_PRINT_ARRAY_EXT(MAC_PAIRED_STA, 6, HEX, ":");
  _FACTORY_DEBUG_PRINTLN("");

  preferences.end();

  _FACTORY_DEBUG_PRINTLN("[NVS] 配置保存成功");
  return true;
}

void debugNvsCheck() {  //     ❌️     ❌️     ❌️     ❌️     ❌️     ❌️     ❌️     ❌️
  preferences.begin("config", true);

  Serial.println("=== NVS 键值检查 ===");

// 检查每个键是否存在
#define CHECK_KEY(key, type)                            \
  if (preferences.isKey(key)) {                         \
    Serial.printf("%-15s [%-7s]: 存在\n", key, type);   \
  } else {                                              \
    Serial.printf("%-15s [%-7s]: 不存在\n", key, type); \
  }

  CHECK_KEY(NVS_KEY_VERSION, "String");

  CHECK_KEY(NVS_KEY_MODE, "uint8");
  CHECK_KEY(NVS_KEY_MIN_SIZE, "uint16");
  CHECK_KEY(NVS_KEY_THRES_SIZE, "uint16");
  CHECK_KEY(NVS_KEY_MIN_TIME, "uint32");
  CHECK_KEY(NVS_KEY_THRES_TIME, "uint32");

  CHECK_KEY(NVS_KEY_SRL_DATA_NUM, "uint8");
  CHECK_KEY(NVS_KEY_SRL_DEBUG_NUM, "uint8");
  CHECK_KEY(NVS_KEY_BAUD_DATA, "uint32");
  CHECK_KEY(NVS_KEY_BAUD_DEBUG, "uint32");
  CHECK_KEY(NVS_KEY_DEBUG_ON, "bool");

  CHECK_KEY(NVS_KEY_WIFI_SSID, "String");
  CHECK_KEY(NVS_KEY_WIFI_PASS, "String");
  CHECK_KEY(NVS_KEY_WIFI_AUTO, "bool");

  CHECK_KEY(NVS_KEY_SND_PORT, "uint16_t");
  CHECK_KEY(NVS_KEY_LSN_PORT, "uint16_t");
  CHECK_KEY(NVS_KEY_STA_IP, "String");
  CHECK_KEY(NVS_KEY_SND_BRD, "bool");
  CHECK_KEY(NVS_KEY_LSN_BRD, "bool");
  CHECK_KEY(NVS_KEY_WIFI_CH, "uint8_t");
  CHECK_KEY(NVS_KEY_WIFI_CH_AT, "bool");
  CHECK_KEY(NVS_KEY_MAC_PAIRED_STA, "Bytes(6)");

  preferences.end();
  Serial.println("=== 检查结束 ===");
}



// Leave Some Spaces.....
