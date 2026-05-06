#include "wifi_config.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/ip_addr.h"
#include <string.h>

// AP 启动完成标志
static volatile bool s_ap_started = false;
static esp_netif_t* s_ap_netif = NULL;



// 外部变量定义
ApSsidPwd apWebCfg_fixed = {AP_NAME_WEB, AP_PWD};
ApSsidPwd apTcp_fixed = {AP_NAME_TCP, AP_PWD};
ApSsidPwd apUdp_fixed = {AP_NAME_UDP, AP_PWD};
ApSsidPwd apEspNow_fixed = {AP_NAME_ESPNOW, AP_PWD};

bool isAPMode = false;
// DNSServer dnsServer;

/**
 * @brief WiFi 事件处理函数 (IDF 风格)
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                DEBUG_PRINTLN("[WIFI EVENT] AP 已启动 ✓");
                s_ap_started = true;
                break;
            case WIFI_EVENT_AP_STOP:
                DEBUG_PRINTLN("[WIFI EVENT] AP 已停止");
                s_ap_started = false;
                break;
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
                DEBUG_PRINTF("[WIFI EVENT] 设备连接, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    event->mac[0], event->mac[1], event->mac[2],
                    event->mac[3], event->mac[4], event->mac[5]);
                deviceConnected = true;
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
                DEBUG_PRINTF("[WIFI EVENT] 设备断开, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    event->mac[0], event->mac[1], event->mac[2],
                    event->mac[3], event->mac[4], event->mac[5]);
                // 检查是否还有其他设备连接
                wifi_sta_list_t sta_list;
                esp_wifi_ap_get_sta_list(&sta_list);
                deviceConnected = (sta_list.num > 0);
                break;
            }
            default:
                break;
        }
    }
}

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

// void startAPMode() {
//   DEBUG_PRINTLN("[WIFI] 启动AP模式");
//   WiFi.disconnect(true);
//   delay(50);
//   WiFi.mode(WIFI_AP);
//   delay(50);

//   IPAddress localIP(AP_IP_COMMA);
//   IPAddress gateway(AP_IP_COMMA);
//   IPAddress subnet(AP_SUBNET_COMMA);

//   WiFi.softAPConfig(localIP, gateway, subnet);

//   // 你的信道选择逻辑非常棒，保留！
//   WIFI_CHNL = WIFI_CH_AUTO ? scanAndSelectChannel() : WIFI_CHNL;

//   // 启动 AP
//   switch (CRNT_MODE) {
//     case MODE_TCP: WiFi.softAP(apTcp_fixed.SSID.c_str(), apTcp_fixed.PassWord.c_str(), WIFI_CHNL, 0, 8); break;
//     case MODE_UDP: WiFi.softAP(apUdp_fixed.SSID.c_str(), apUdp_fixed.PassWord.c_str(), WIFI_CHNL, 0, 8); break;
//     case MODE_ESPNOW: WiFi.softAP(apEspNow_fixed.SSID.c_str(), apEspNow_fixed.PassWord.c_str(), WIFI_CHNL, 0, 8); break;
//     default: WiFi.softAP(apWebCfg_fixed.SSID.c_str(), apWebCfg_fixed.PassWord.c_str(), WIFI_CHNL, 0, 8);
//   }
//   delay(50);

//   // // 启动 DNS 服务（用于劫持所有域名到 10.0.0.1）
//   // dnsServer.start(53, "*", localIP);

//   optimize_wifi_performance();  // AP 模式同样需要性能优化
//   isAPMode = true;
// }
void startAPMode() {
  DEBUG_PRINTLN("[WIFI] 启动AP模式 (IDF 风格)");
  
  // ═══════════════════════════════════════════════════
  // 第零步：信道扫描（必须在关闭 Arduino WiFi 之前）
  // ═══════════════════════════════════════════════════
  if (WIFI_CH_AUTO) {
      WiFi.mode(WIFI_STA);
      delay(100);
      WIFI_CHNL = scanAndSelectChannel();
      DEBUG_PRINTF("[WIFI] 自动选择信道: %d\n", WIFI_CHNL);
  }
  
  // ═══════════════════════════════════════════════════
  // 第一步：完全停止 Arduino WiFi（防止状态冲突）
  // ═══════════════════════════════════════════════════
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  
  // ═══════════════════════════════════════════════════
  // 第二步：初始化 NVS（WiFi 驱动需要）
  // ═══════════════════════════════════════════════════
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      nvs_flash_erase();
      nvs_flash_init();
  }
  
  // ═══════════════════════════════════════════════════
  // 第三步：初始化 TCP/IP 协议栈和事件循环
  // ═══════════════════════════════════════════════════
  ESP_ERROR_CHECK(esp_netif_init());
  
  // 创建默认事件循环（如果还没创建）
  esp_err_t err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      DEBUG_PRINTF("[WIFI] 事件循环创建失败: %d\n", err);
  }
  
  // 创建 AP 网络接口
  s_ap_netif = esp_netif_create_default_wifi_ap();
  
  // ═══════════════════════════════════════════════════
  // 第四步：设置静态 IP
  // ═══════════════════════════════════════════════════
  esp_netif_dhcps_stop(s_ap_netif);  // 先停止 DHCP 服务器
  
  esp_netif_ip_info_t ip_info;
  memset(&ip_info, 0, sizeof(ip_info));
  IP4_ADDR(&ip_info.ip, 10, 0, 0, 1);
  IP4_ADDR(&ip_info.gw, 10, 0, 0, 1);
  IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
  esp_netif_set_ip_info(s_ap_netif, &ip_info);
  
  esp_netif_dhcps_start(s_ap_netif);  // 重新启动 DHCP 服务器
  
  // ═══════════════════════════════════════════════════
  // 第五步：初始化 WiFi 驱动
  // ═══════════════════════════════════════════════════
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  
  // 注册事件处理函数
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  
  // ═══════════════════════════════════════════════════
  // 第六步：配置 AP 参数
  // ═══════════════════════════════════════════════════
  // 信道已在第零步确定
  
  wifi_config_t ap_config = {};
  ap_config.ap.channel = WIFI_CHNL;
  ap_config.ap.max_connection = 8;
  ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  
  // 根据模式选择 SSID
  const char* ssid = NULL;
  const char* password = AP_PWD;
  switch (CRNT_MODE) {
    case MODE_TCP:    ssid = apTcp_fixed.SSID.c_str(); password = apTcp_fixed.PassWord.c_str(); break;
    case MODE_UDP:    ssid = apUdp_fixed.SSID.c_str(); password = apUdp_fixed.PassWord.c_str(); break;
    case MODE_ESPNOW: ssid = apEspNow_fixed.SSID.c_str(); password = apEspNow_fixed.PassWord.c_str(); break;
    default:          ssid = apWebCfg_fixed.SSID.c_str(); password = apWebCfg_fixed.PassWord.c_str();
  }
  
  strncpy((char*)ap_config.ap.ssid, ssid, sizeof(ap_config.ap.ssid) - 1);
  ap_config.ap.ssid_len = strlen(ssid);
  strncpy((char*)ap_config.ap.password, password, sizeof(ap_config.ap.password) - 1);
  
  DEBUG_PRINTF("[WIFI] SSID: %s, 信道: %d\n", ssid, WIFI_CHNL);
  
  // ═══════════════════════════════════════════════════
  // 第七步：启动 WiFi
  // ═══════════════════════════════════════════════════
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
  
  s_ap_started = false;
  ESP_ERROR_CHECK(esp_wifi_start());
  
  // 等待 AP 启动事件
  DEBUG_PRINTLN("[WIFI] 等待 AP_START 事件...");
  uint32_t timeout = millis() + 5000;
  while (!s_ap_started && millis() < timeout) {
      delay(10);
  }
  
  if (!s_ap_started) {
      DEBUG_PRINTLN("[WIFI] ❌ AP 启动超时！");
      return;
  }
  
  // ═══════════════════════════════════════════════════
  // 第八步：性能优化
  // ═══════════════════════════════════════════════════
  
  // 强制 11b 协议 + 1Mbps DBPSK
  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B);
  esp_wifi_config_80211_tx_rate(WIFI_IF_AP, WIFI_PHY_RATE_1M_L);
  
  // 关闭省电
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setSleep(false);
  
  // 最大发射功率
  esp_wifi_set_max_tx_power(80);
  
  // 设置国家码（允许 1-13 信道）
  wifi_country_t country = {
      .cc = "CN",
      .schan = 1,
      .nchan = 13,
      .policy = WIFI_COUNTRY_POLICY_MANUAL
  };
  esp_wifi_set_country(&country);
  
  isAPMode = true;
  deviceConnected = false;  // 等待设备连接事件
  
  DEBUG_PRINTLN("[WIFI] ✅ AP 模式启动完成 (IDF 风格)");
  DEBUG_PRINTF("[WIFI] IP: 10.0.0.1, 信道: %d, 协议: 11b DBPSK\n", WIFI_CHNL);
}

void checkWifiConnection() {
  // STA 模式：用 Arduino API 检查
  if (WiFi.status() == WL_CONNECTED) {
    deviceConnected = true;
    return;
  }
  
  // AP 模式（IDF 风格）：用 esp_wifi 检查
  if (isAPMode) {
    wifi_sta_list_t sta_list;
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
      deviceConnected = (sta_list.num > 0);
    }
    return;
  }
  
  // 其他情况
  deviceConnected = false;
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
