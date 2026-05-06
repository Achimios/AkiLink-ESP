
#include "wifi_config.h"

ApSsidPwd apWebCfg_fixed = { AP_NAME_WEB, AP_PWD };
ApSsidPwd apTcp_fixed = { AP_NAME_TCP, AP_PWD };
ApSsidPwd apUdp_fixed = { AP_NAME_UDP, AP_PWD };
ApSsidPwd apEspNow_fixed = { AP_NAME_ESPNOW, AP_PWD };

bool isAPMode = false;

DNSServer dnsServer;

// 网络连接函数
bool connectToSTA() {
  if (!HOME_WIFI_AUTO || HOME_WIFI_SSID.length() == 0) {
    DEBUG_PRINTLN("[UDP] 未配置自动连接WiFi");
    return false;
  }

  DEBUG_PRINTLN("[UDP] 尝试连接STA: " + HOME_WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(HOME_WIFI_SSID.c_str(), HOME_WIFI_PWD.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(100);
    DEBUG_PRINT(".");
  }
  DEBUG_PRINTLN();

  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_PRINTLN("[UDP] ✅ STA连接成功");
    return true;
  } else {
    DEBUG_PRINTLN("[UDP] ❌ STA连接失败");
    WiFi.disconnect();
    return false;
  }
}


void startAPMode() {
  DEBUG_PRINTLN("[UDP] 启动AP模式回退");
  WiFi.disconnect(true);
  delay(50);
  WiFi.mode(WIFI_AP);
  delay(50);

  // 配置AP参数
  // wifi_config_t wifi_config;                                             //临时实例
  // memset(&wifi_config, 0, sizeof(wifi_config));                          //初始化
  // strcpy((char*)wifi_config.ap.ssid, apUdp_fixed.SSID.c_str());          //AP SSID
  // wifi_config.ap.ssid_len = strlen(apUdp_fixed.SSID.c_str());            // SSID 长度
  // strcpy((char*)wifi_config.ap.password, apUdp_fixed.PassWord.c_str());  //密码
  // wifi_config.ap.channel = WIFI_CH_AUTO ? scanAndSelectChannel() : WIFI_CHNL;  // 通常13频道最干净
  // wifi_config.ap.max_connection = 8;     //最多接入8设备
  // wifi_config.ap.beacon_interval = 100;  // 信标间隔（默认100）
  // // 关键：设置AP为"开放系统"，可能改善广播
  // wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK; // 如果 password 长度合法（8-63字节）→ 强制使用 WIFI_AUTH_WPA2_PSK。
  // 📢就是改这里📢改成无密码后，AP时还是无法接收广播，所谓了
  // wifi_config.ap.authmode = WIFI_AUTH_OPEN;  // 如果 password 为 NULL 或空字符串 "" → 强制使用 WIFI_AUTH_OPEN。
  // //改完保存
  // esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
  // esp_wifi_start();

  IPAddress localIP(AP_IP_COMMA);
  IPAddress gateway(AP_IP_COMMA);
  IPAddress subnet(AP_SUBNET_COMMA);

  WiFi.softAPConfig(localIP, gateway, subnet);
  WIFI_CHNL = WIFI_CH_AUTO ? scanAndSelectChannel() : WIFI_CHNL;  // 通常13频道最干净
  WiFi.softAP(apUdp_fixed.SSID.c_str(), apUdp_fixed.PassWord.c_str(), WIFI_CHNL, 0, 8);
  // bool softAP(const char* ssid, const char* passphrase = nullptr, int channel = 1, int ssid_hidden = 0,
  // int max_connection = 4, bool ftm_responder = false); // ftm_responder为室内高精度定位
  delay(50);
  dnsServer.start(53, "*", localIP);
  isAPMode = true;
}


uint8_t scanAndSelectChannel() {
  int channelScore[14] = { 0 };
  DEBUG_PRINTLN("扫描附近AP热点...");
  uint16_t scanResults = WiFi.scanNetworks();
  DEBUG_PRINTLN("检测热点干扰强度...");
  for (int i = 0; i < scanResults; i++) {
    int ch = WiFi.channel(i);
    int rssi = WiFi.RSSI(i);

    // 信号越强，干扰越大
    int penalty = abs(rssi);

    // 主信道
    channelScore[ch] += penalty;

    // 邻近信道（20MHz overlap）
    if (ch > 1) channelScore[ch - 1] += penalty / 2;
    if (ch < 13) channelScore[ch + 1] += penalty / 2;
  }

  uint8_t bestCh = 1;
  for (uint8_t ch = 1; ch <= 13; ch++) {
    if (channelScore[ch] < channelScore[bestCh]) {
      bestCh = ch;
    }
  }
  DEBUG_PRINT("选择干扰最小信道为：" + String(bestCh));
  return bestCh;
}

void getLocalMacs() {
    // 獲取 STA 模式的 MAC
    esp_read_mac(self_sta_mac, ESP_MAC_WIFI_STA);
    // 獲取 AP 模式的 MAC
    esp_read_mac(self_ap_mac, ESP_MAC_WIFI_SOFTAP);
    DEBUG_PRINTLN("本机STA MAC地址:");
    DEBUG_PRINT_ARRAY_EXT(self_sta_mac, 6, HEX, ":");
    DEBUG_PRINTLN();
    DEBUG_PRINTLN("本机AP MAC地址:");
    DEBUG_PRINT_ARRAY_EXT(self_ap_mac, 6, HEX, ":");
    DEBUG_PRINTLN();
}

void checkWifiConnection(){
    // 检查 STA 连接
  if (WiFi.status() == WL_CONNECTED)
  {
    deviceConnected = true;
  }
  // 检查 AP 连接
  else if (WiFi.getMode() & WIFI_AP)
  {
    if (WiFi.softAPgetStationNum() > 0)
    {
      deviceConnected = true;
    }
    else
    {
      deviceConnected = false;
    }
  }
  else
  {
    deviceConnected = false;
  }
}