#include "wifi_config.h"

#include <esp_wifi.h>

// 外部变量定义
ApSsidPwd apWebCfg_fixed = {AP_NAME_WEB, AP_PWD};
ApSsidPwd apTcp_fixed = {AP_NAME_TCP, AP_PWD};
ApSsidPwd apUdp_fixed = {AP_NAME_UDP, AP_PWD};
ApSsidPwd apEspNow_fixed = {AP_NAME_ESPNOW, AP_PWD};

bool isAPMode = false;
// DNSServer dnsServer;

/**
 * @brief WiFi 极限性能优化
 * 核心：关闭 WiFi 的 Modem-sleep 模式，降低网络延迟
 */
static void optimize_wifi_performance() {
  // 关闭 WiFi 省电模式。默认情况下，ESP32 会为了省电进入 Modem-sleep，
  // 这会导致网络响应延迟增加（ping 值跳动）。透传必须关闭它！
  esp_wifi_set_ps(WIFI_PS_NONE);
  if (isAPMode)
    esp_wifi_config_80211_tx_rate(WIFI_IF_AP, WIFI_PHY_RATE_1M_L);
  else
    esp_wifi_config_80211_tx_rate(WIFI_IF_STA, WIFI_PHY_RATE_1M_L);
  esp_wifi_set_max_tx_power(80);

  // 启动 mDNS。这样你就可以通过 http://esp-air.local 访问了 。
  // 它在 UDP/TCP 模式下都有效。
  // if (MDNS.begin("esp-air")) {   //无效，直接注释
  //     DEBUG_PRINTLN("[WIFI] mDNS 启动成功: esp-air.local");
  // }
}

// 网络连接函数
bool connectToSTA() {
  if (!HOME_WIFI_AUTO || HOME_WIFI_SSID.length() == 0) {
    DEBUG_PRINTLN("[WIFI] 未配置自动连接WiFi");
    return false;
  }

  DEBUG_PRINTLN("[WIFI] 尝试连接STA: " + HOME_WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(HOME_WIFI_SSID.c_str(), HOME_WIFI_PWD.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(100);
    DEBUG_PRINT(".");
  }
  DEBUG_PRINTLN();

  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_PRINTLN("[WIFI] ✅ STA连接成功");
    optimize_wifi_performance();  // 连上后立即执行性能优化
    return true;
  } else {
    DEBUG_PRINTLN("[WIFI] ❌ STA连接失败");
    WiFi.disconnect();
    return false;
  }
}

void startAPMode() {
  DEBUG_PRINTLN("[WIFI] 启动AP模式");
  WiFi.disconnect(true);
  delay(50);
  WiFi.mode(WIFI_AP);
  delay(50);

  IPAddress localIP(AP_IP_COMMA);
  IPAddress gateway(AP_IP_COMMA);
  IPAddress subnet(AP_SUBNET_COMMA);

  WiFi.softAPConfig(localIP, gateway, subnet);

  // 你的信道选择逻辑非常棒，保留！
  WIFI_CHNL = WIFI_CH_AUTO ? scanAndSelectChannel() : WIFI_CHNL;

  // 启动 AP
  switch (CRNT_MODE) {
    case MODE_TCP: WiFi.softAP(apTcp_fixed.SSID.c_str(), apTcp_fixed.PassWord.c_str(), WIFI_CHNL, 0, 8); break;
    case MODE_UDP: WiFi.softAP(apUdp_fixed.SSID.c_str(), apUdp_fixed.PassWord.c_str(), WIFI_CHNL, 0, 8); break;
    case MODE_ESPNOW: WiFi.softAP(apEspNow_fixed.SSID.c_str(), apEspNow_fixed.PassWord.c_str(), WIFI_CHNL, 0, 8); break;
    default: WiFi.softAP(apWebCfg_fixed.SSID.c_str(), apWebCfg_fixed.PassWord.c_str(), WIFI_CHNL, 0, 8);
  }
  delay(50);
  
  // // 启动 DNS 服务（用于劫持所有域名到 10.0.0.1）
  // dnsServer.start(53, "*", localIP);

  optimize_wifi_performance();  // AP 模式同样需要性能优化
  isAPMode = true;
}

void checkWifiConnection() {
  // 检查 STA 连接
  if (WiFi.status() == WL_CONNECTED) {
    deviceConnected = true;
  }
  // 检查 AP 连接
  else if (WiFi.getMode() & WIFI_AP) {
    // 如果有设备连上我们的热点，也认为 deviceConnected
    if (WiFi.softAPgetStationNum() > 0) {
      deviceConnected = true;
    } else {
      deviceConnected = false;
  }
  } else {
  deviceConnected = false;
  }
}

uint8_t scanAndSelectChannel() {
  // ... 你的信道扫描逻辑保持不变，这部分写得很专业 ...
  // (此处省略具体实现以节省篇幅，实际代码中请保留)
  int channelScore[14] = {0};
  uint16_t scanResults = WiFi.scanNetworks();
  for (int i = 0; i < scanResults; i++) {
    int ch = WiFi.channel(i);
    int rssi = WiFi.RSSI(i);
    int penalty = abs(rssi);
    channelScore[ch] += penalty;
    if (ch > 1) channelScore[ch - 1] += penalty / 2;
    if (ch < 13) channelScore[ch + 1] += penalty / 2;
  }
  uint8_t bestCh = 1;
  for (uint8_t ch = 1; ch <= 13; ch++) {
    if (channelScore[ch] < channelScore[bestCh]) bestCh = ch;
  }
  return bestCh;
}

void getLocalMacs() {
  esp_read_mac(self_sta_mac, ESP_MAC_WIFI_STA);
  esp_read_mac(self_ap_mac, ESP_MAC_WIFI_SOFTAP);
  // ... 打印逻辑保持不变 ...
}
