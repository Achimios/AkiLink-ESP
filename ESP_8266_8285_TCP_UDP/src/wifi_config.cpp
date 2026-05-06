// ============================================================
//  ESP8266 / ESP8285  wifi_config.cpp
//  Ported from ESP32_Transmission_5Modes_V3
// ============================================================

#include "wifi_config.h"

bool isAPMode = false;


// ============================================================
//  WiFi 性能优化
// ============================================================
// ESP8266 没有 esp_wifi_set_ps() / esp_wifi_config_80211_tx_rate() 这些IDF API
// 但 Arduino Core 提供了等价接口
static void optimize_wifi_performance() {
    // 关闭 WiFi Modem-sleep — 透传必须关！否则延迟跳动
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    // WIFI_NONE_SLEEP   = 完全不睡  (最低延迟，功耗最高 ~70mA)
    // WIFI_LIGHT_SLEEP  = 轻度睡眠  (省电但延迟增加)
    // WIFI_MODEM_SLEEP  = 调制解调器睡眠 (默认，DTIM间隔醒来)

    // ESP8266 没有 esp_wifi_config_80211_tx_rate()
    // 无法像ESP32那样强制1Mbps DBPSK长距离模式
    // 但可以设置最大发射功率 (默认20.5dBm，已是最大)
    WiFi.setOutputPower(20.5);  // 单位dBm，范围0~20.5

    // ESP8266 不支持 mDNS 的稳定实现，不启用
}


// ============================================================
//  connectToSTA() — 连接家庭WiFi
// ============================================================
bool connectToSTA() {
    if (!HOME_WIFI_AUTO || HOME_WIFI_SSID.length() == 0) {
        DEBUG_PRINTLN("[WIFI] No home WiFi configured");
        return false;
    }

    _FACTORY_PRINTF("[WIFI] Connecting STA: \"%s\"\n", HOME_WIFI_SSID.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(HOME_WIFI_SSID.c_str(), HOME_WIFI_PWD.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(100);
        DEBUG_PRINT(".");
    }
    DEBUG_PRINTLN("");

    if (WiFi.status() == WL_CONNECTED) {
        _FACTORY_PRINTF("[WIFI] STA connected! IP=%s\n", WiFi.localIP().toString().c_str());
        optimize_wifi_performance();
        isAPMode = false;
        return true;
    } else {
        DEBUG_PRINTLN("[WIFI] STA connect FAILED");
        WiFi.disconnect();
        return false;
    }
}


// ============================================================
//  startAPMode() — 启动热点
// ============================================================
void startAPMode() {
    DEBUG_PRINTLN("[WIFI] Starting AP mode...");
    WiFi.disconnect(true);
    delay(50);
    WiFi.mode(WIFI_AP);
    delay(50);

    IPAddress localIP(AP_IP_COMMA);
    IPAddress gateway(AP_IP_COMMA);
    IPAddress subnet(AP_SUBNET_COMMA);

    WiFi.softAPConfig(localIP, gateway, subnet);

    // 信道选择
    WIFI_CHNL = WIFI_CH_AUTO ? scanAndSelectChannel() : WIFI_CHNL;

    // 根据当前模式选择AP名称
    const char* apName;
    switch (CRNT_MODE) {
        case MODE_TCP: apName = AP_NAME_TCP; break;
        case MODE_UDP: apName = AP_NAME_UDP; break;
        default:       apName = AP_NAME_WEB; break;
    }

    // softAP(ssid, pwd, channel, hidden, max_connection)
    // ESP8266 最多支持 8 个STA连接 (实际建议<=4)
    WiFi.softAP(apName, AP_PWD, WIFI_CHNL, 0, 4);
    delay(50);

    optimize_wifi_performance();
    isAPMode = true;

    _FACTORY_PRINTF("[WIFI] AP started: \"%s\" Ch=%d IP=%s\n",
                    apName, WIFI_CHNL, WiFi.softAPIP().toString().c_str());
}


// ============================================================
//  checkWifiConnection() — 检查连接状态
// ============================================================
void checkWifiConnection() {
    if (WiFi.getMode() & WIFI_STA) {
        // STA 模式
        deviceConnected = (WiFi.status() == WL_CONNECTED);
    } else if (WiFi.getMode() & WIFI_AP) {
        // AP 模式
        deviceConnected = (WiFi.softAPgetStationNum() > 0);
    } else {
        deviceConnected = false;
    }
}


// ============================================================
//  scanAndSelectChannel() — 扫描选择最优信道
// ============================================================
// 给每个信道打分：周围AP越多/越强，惩罚越高。选惩罚最低的信道。
// 相邻信道也施加半衰惩罚（WiFi 2.4G信道间有重叠）
uint8_t scanAndSelectChannel() {
    _FACTORY_DEBUG_PRINTLN("[WIFI] Scanning channels...");

    int channelScore[14] = {0};  // index 0 不用，1-13 对应信道
    int16_t scanResults = WiFi.scanNetworks();

    _FACTORY_PRINTF("[WIFI] Found %d networks\n", scanResults);

    for (int i = 0; i < scanResults; i++) {
        int ch   = WiFi.channel(i);
        int rssi = WiFi.RSSI(i);
        int penalty = abs(rssi);  // RSSI是负数，信号越强惩罚越大

        // 当前信道全额惩罚
        if (ch >= 1 && ch <= 13) channelScore[ch] += penalty;
        // 相邻信道半额惩罚 (2.4G信道22MHz带宽，5MHz间隔 → 相邻有重叠)
        if (ch > 1)  channelScore[ch - 1] += penalty / 2;
        if (ch < 13) channelScore[ch + 1] += penalty / 2;
    }

    WiFi.scanDelete();  // 释放扫描结果内存 (8266 RAM紧张!)

    // 选最低分 = 最清净的信道
    uint8_t bestCh = 1;
    for (uint8_t ch = 1; ch <= 13; ch++) {
        if (channelScore[ch] < channelScore[bestCh]) bestCh = ch;
    }

    _FACTORY_PRINTF("[WIFI] Best channel: %d (score=%d)\n", bestCh, channelScore[bestCh]);
    return bestCh;
}


// ============================================================
//  getLocalMacs() — 获取自身MAC地址
// ============================================================
void getLocalMacs() {
    // ESP8266 Arduino API: WiFi.macAddress() 返回STA MAC
    // AP MAC = STA MAC 的最后一个字节 +1 (ESP官方规则)
    WiFi.macAddress(self_sta_mac);
    WiFi.softAPmacAddress(self_ap_mac);

    _FACTORY_PRINTF("[WIFI] STA MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    self_sta_mac[0], self_sta_mac[1], self_sta_mac[2],
                    self_sta_mac[3], self_sta_mac[4], self_sta_mac[5]);
    _FACTORY_PRINTF("[WIFI] AP  MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    self_ap_mac[0], self_ap_mac[1], self_ap_mac[2],
                    self_ap_mac[3], self_ap_mac[4], self_ap_mac[5]);
}


// Leave Some Spaces.....
