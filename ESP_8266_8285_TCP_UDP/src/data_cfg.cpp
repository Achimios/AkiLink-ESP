// ============================================================
//  ESP8266 / ESP8285  data_cfg.cpp
//  持久化存储: LittleFS + ArduinoJson (取代ESP32的NVS/Preferences)
//  配置文件路径: /config.json
// ============================================================

#include "data_cfg.h"

#include <LittleFS.h>
#include <ArduinoJson.h>  // 需要 lib_deps = bblanchon/ArduinoJson @ ^6.21.0

#define CONFIG_FILE "/config.json"


// ============================================================
//  🐦🐦 默认配置定义 🐦🐦
// ============================================================
// 版本标识 — 数据结构变更时递增，触发格式化重写
String CfgReadyTag_fixed = "V_1_1";

// AirConfig 默认值 (由 SET_DFT_CONFIGS 宏展开)
SET_DFT_CONFIGS

// 串口默认值 — ESP8266固定映射，只有波特率和调试开关可配
#ifdef _FACTORY_DEBUG
SerialCfg serial_DFT = {DFT_BAUD, FCT_BAUD_DBG, true};
#else
SerialCfg serial_DFT = {DFT_BAUD, DFT_BAUD, false};
#endif

// WiFi/端口默认值
WifiIpPort wifiIpPort_dft = {14550, 5761, "192.168.x.x", true, true, 13, false};


// ============================================================
//  🌟🌟 当前运行时配置 (先复制默认值) 🌟🌟
// ============================================================
Mode crnt_Mode                  = MODE_TCP;
AirConfig crnt_AirCfg           = tcpCfg_dft;
SerialCfg crnt_SerialCfg        = serial_DFT;
WifiIpPort crnt_wifiIpPort      = wifiIpPort_dft;
HomeWifiConfig crnt_HomeWifiCfg = {"", "", false};

// 按钮/Web模式
bool lastCfgPin      = false;
bool web_config_mode = false;


// ============================================================
//  🐻🐻 当前连接状态 🐻🐻
// ============================================================
uint8_t self_sta_mac[6];
uint8_t self_ap_mac[6];

bool tcpClientConnected = false;
bool deviceConnected    = false;
bool oldDeviceConnected = false;


// ============================================================
//  🔧 LittleFS JSON 键名
// ============================================================
// 不受NVS 15字符限制，但保持简洁便于阅读JSON文件
static const char* KEY_VER       = "ver";
static const char* KEY_MODE      = "mode";
static const char* KEY_MIN_SZ    = "min_sz";
static const char* KEY_THR_SZ    = "thr_sz";
static const char* KEY_MIN_TM    = "min_tm";
static const char* KEY_THR_TM    = "thr_tm";
static const char* KEY_BAUD_DAT  = "baud_dat";
static const char* KEY_BAUD_DBG  = "baud_dbg";
static const char* KEY_DBG_ON    = "dbg_on";
static const char* KEY_WIFI_SSID = "wifi_ssid";
static const char* KEY_WIFI_PASS = "wifi_pass";
static const char* KEY_WIFI_AUTO = "wifi_auto";
static const char* KEY_SND_PORT  = "snd_port";
static const char* KEY_LSN_PORT  = "lsn_port";
static const char* KEY_STA_IP    = "sta_ip";
static const char* KEY_SND_BRD   = "snd_brd";
static const char* KEY_LSN_BRD   = "lsn_brd";
static const char* KEY_WIFI_CH   = "wifi_ch";
static const char* KEY_WIFI_CHAT = "wifi_chat";


// ============================================================
//  🔧 checkStoredConfig()  — 开机调用
//  流程: dft→crnt (已在全局初始化完成) → 读版本号 → 不匹配则store → read
// ============================================================
void checkStoredConfig() {
    _FACTORY_DEBUG_PRINTLN("[CFG] checkStoredConfig...");

    if (!LittleFS.begin()) {
        _FACTORY_DEBUG_PRINTLN("[CFG] LittleFS mount failed! Formatting...");
        LittleFS.format();
        if (!LittleFS.begin()) {
            _FACTORY_DEBUG_PRINTLN("[CFG] LittleFS mount STILL failed after format!");
            return;
        }
    }

    // 尝试读取版本号
    String storedVer = "";
    if (LittleFS.exists(CONFIG_FILE)) {
        File f = LittleFS.open(CONFIG_FILE, "r");
        if (f) {
            StaticJsonDocument<1024> doc;
            DeserializationError err = deserializeJson(doc, f);
            f.close();
            if (!err && doc.containsKey(KEY_VER)) {
                storedVer = doc[KEY_VER].as<String>();
            }
        }
    }

    _FACTORY_PRINTF("[CFG] Stored ver: \"%s\", Expected: \"%s\"\n",
                    storedVer.c_str(), CfgReadyTag_fixed.c_str());

    if (storedVer != CfgReadyTag_fixed) {
        _FACTORY_DEBUG_PRINTLN("[CFG] Version mismatch or first boot. Storing defaults...");
        // crnt已经是dft了(全局初始化)，直接存
        storeDataConfig();
    }

    // 最后读取 — 无论是否刚写入，都从文件读一遍，确保crnt与文件同步
    readDataConfig();
}


// ============================================================
//  🔧 readDataConfig()  — 从 /config.json 读取到 crnt_xxx
// ============================================================
void readDataConfig() {
    _FACTORY_DEBUG_PRINTLN("[CFG] Reading config...");

    File f = LittleFS.open(CONFIG_FILE, "r");
    if (!f) {
        _FACTORY_DEBUG_PRINTLN("[CFG] Cannot open config file for reading!");
        return;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        _FACTORY_PRINTF("[CFG] JSON parse error: %s\n", err.c_str());
        return;
    }

    // 📌 模式
    uint8_t mode_val = doc[KEY_MODE] | (uint8_t)MODE_UDP;
    CRNT_MODE = static_cast<Mode>(mode_val);
    if (CRNT_MODE != MODE_TCP && CRNT_MODE != MODE_UDP) {
        CRNT_MODE = MODE_UDP;  // 安全回退
    }

    // 📌 AirConfig — 根据模式选择对应默认值作为fallback
    AirConfig& fallback = (CRNT_MODE == MODE_TCP) ? tcpCfg_dft : udpCfg_dft;
    FLUSH_MIN_SIZE      = doc[KEY_MIN_SZ]  | fallback.flush_min_size;
    FLUSH_THRES_SIZE    = doc[KEY_THR_SZ]  | fallback.flush_thres_size;
    FLUSH_MIN_TIME_US   = doc[KEY_MIN_TM]  | fallback.flush_min_time_us;
    FLUSH_THRES_TIME_US = doc[KEY_THR_TM]  | fallback.flush_thres_time_us;

    // 📌 串口配置 (ESP8266不切换串口号，只存波特率和调试开关)
    BAUD_DATA  = doc[KEY_BAUD_DAT] | (uint32_t)DFT_BAUD;
    BAUD_DEBUG = doc[KEY_BAUD_DBG] | (uint32_t)FCT_BAUD_DBG;
    DEBUG_ON   = doc[KEY_DBG_ON]   | serial_DFT.DebugON;

    // 📌 家庭WiFi
    HOME_WIFI_SSID = doc[KEY_WIFI_SSID] | "";
    HOME_WIFI_PWD  = doc[KEY_WIFI_PASS] | "";
    HOME_WIFI_AUTO = doc[KEY_WIFI_AUTO] | false;

    // 📌 端口 / 信道
    PC_UDP_PORT  = doc[KEY_SND_PORT]  | wifiIpPort_dft.sendPort;
    ESP_UDP_PORT = doc[KEY_LSN_PORT]  | wifiIpPort_dft.listenPort;
    ESP_STA_IP   = doc[KEY_STA_IP]    | "192.168.x.x";
    SEND_BROAD   = doc[KEY_SND_BRD]   | true;
    LISTEN_BROAD = doc[KEY_LSN_BRD]   | true;
    WIFI_CHNL    = doc[KEY_WIFI_CH]   | wifiIpPort_dft.wifiChannel;
    WIFI_CH_AUTO = doc[KEY_WIFI_CHAT] | false;

    // 📌 打印
    _FACTORY_PRINTF("[CFG] Mode=%d  Air: %u/%u bytes, %lu/%lu us\n",
                    CRNT_MODE, FLUSH_MIN_SIZE, FLUSH_THRES_SIZE,
                    FLUSH_MIN_TIME_US, FLUSH_THRES_TIME_US);
    _FACTORY_PRINTF("[CFG] BaudData=%lu BaudDbg=%lu Debug=%s\n",
                    BAUD_DATA, BAUD_DEBUG, DEBUG_ON ? "ON" : "OFF");
    _FACTORY_PRINTF("[CFG] HomeWiFi SSID=\"%s\" Auto=%s\n",
                    HOME_WIFI_SSID.c_str(), HOME_WIFI_AUTO ? "Y" : "N");
    _FACTORY_PRINTF("[CFG] Send:%u Listen:%u Ch=%d ChAuto=%s\n",
                    PC_UDP_PORT, ESP_UDP_PORT, WIFI_CHNL, WIFI_CH_AUTO ? "Y" : "N");
    _FACTORY_PRINTF("[CFG] SendBrd=%s ListenBrd=%s staIP=%s\n",
                    SEND_BROAD ? "Y" : "N", LISTEN_BROAD ? "Y" : "N", ESP_STA_IP.c_str());

    _FACTORY_DEBUG_PRINTLN("[CFG] Config read OK.");
}


// ============================================================
//  🔧 storeDataConfig()  — 将 crnt_xxx 写入 /config.json
//  Web页面Apply或首次格式化时调用
// ============================================================
bool storeDataConfig() {
    _FACTORY_DEBUG_PRINTLN("[CFG] Storing config...");

    StaticJsonDocument<1024> doc;

    // 版本号
    doc[KEY_VER] = CfgReadyTag_fixed;

    // 模式
    doc[KEY_MODE] = (uint8_t)CRNT_MODE;

    // AirConfig
    doc[KEY_MIN_SZ] = FLUSH_MIN_SIZE;
    doc[KEY_THR_SZ] = FLUSH_THRES_SIZE;
    doc[KEY_MIN_TM] = FLUSH_MIN_TIME_US;
    doc[KEY_THR_TM] = FLUSH_THRES_TIME_US;

    // 串口
    doc[KEY_BAUD_DAT] = BAUD_DATA;
    doc[KEY_BAUD_DBG] = BAUD_DEBUG;
    doc[KEY_DBG_ON]   = DEBUG_ON;

    // 家庭WiFi
    doc[KEY_WIFI_SSID] = HOME_WIFI_SSID;
    doc[KEY_WIFI_PASS] = HOME_WIFI_PWD;
    doc[KEY_WIFI_AUTO] = HOME_WIFI_AUTO;

    // 端口 / 信道
    doc[KEY_SND_PORT]  = PC_UDP_PORT;
    doc[KEY_LSN_PORT]  = ESP_UDP_PORT;
    doc[KEY_STA_IP]    = ESP_STA_IP;
    doc[KEY_SND_BRD]   = SEND_BROAD;
    doc[KEY_LSN_BRD]   = LISTEN_BROAD;
    doc[KEY_WIFI_CH]   = WIFI_CHNL;
    doc[KEY_WIFI_CHAT] = WIFI_CH_AUTO;

    // 写入文件
    File f = LittleFS.open(CONFIG_FILE, "w");
    if (!f) {
        _FACTORY_DEBUG_PRINTLN("[CFG] Cannot open config file for writing!");
        return false;
    }

    size_t written = serializeJson(doc, f);
    f.close();

    _FACTORY_PRINTF("[CFG] Written %u bytes to %s\n", written, CONFIG_FILE);

    // 打印确认
    _FACTORY_PRINTF("[CFG] Stored: Mode=%d  Air: %u/%u, %lu/%lu us\n",
                    CRNT_MODE, FLUSH_MIN_SIZE, FLUSH_THRES_SIZE,
                    FLUSH_MIN_TIME_US, FLUSH_THRES_TIME_US);
    _FACTORY_PRINTF("[CFG] Stored: Baud=%lu/%lu Dbg=%s\n",
                    BAUD_DATA, BAUD_DEBUG, DEBUG_ON ? "ON" : "OFF");
    _FACTORY_PRINTF("[CFG] Stored: WiFi=\"%s\" Ports=%u/%u Ch=%d\n",
                    HOME_WIFI_SSID.c_str(), PC_UDP_PORT, ESP_UDP_PORT, WIFI_CHNL);

    _FACTORY_DEBUG_PRINTLN("[CFG] Config stored OK.");
    return true;
}


// ============================================================
//  🔧 debugConfigCheck()  — 仅 _FACTORY_DEBUG 下使用
//  直接读文件并打印raw JSON，方便调试
// ============================================================
void debugConfigCheck() {
#ifdef _FACTORY_DEBUG
    S_DEBUG.println("=== LittleFS Config Check ===");

    if (!LittleFS.exists(CONFIG_FILE)) {
        S_DEBUG.println("Config file NOT found!");
        S_DEBUG.println("=== Check End ===");
        return;
    }

    File f = LittleFS.open(CONFIG_FILE, "r");
    if (!f) {
        S_DEBUG.println("Cannot open config file!");
        S_DEBUG.println("=== Check End ===");
        return;
    }

    S_DEBUG.println("--- Raw JSON ---");
    while (f.available()) {
        S_DEBUG.write(f.read());
    }
    S_DEBUG.println("\n--- End JSON ---");
    f.close();

    // 文件系统信息
    FSInfo fs_info;
    LittleFS.info(fs_info);
    S_DEBUG.printf("FS: used %u / %u bytes (%.1f%%)\n",
                   fs_info.usedBytes, fs_info.totalBytes,
                   100.0f * fs_info.usedBytes / fs_info.totalBytes);

    S_DEBUG.println("=== Check End ===");
#endif
}


// Leave Some Spaces.....
