/**
 * ═══════════════════════════════════════════════════════════════
 *  wifi_setup.c — Wi-Fi 初始化与管理实现（ESP-IDF 版）
 * ═══════════════════════════════════════════════════════════════
 *
 *  移植自原 Arduino 项目的 wifi_config.cpp
 *
 *  IDF Wi-Fi 初始化和 Arduino WiFi.begin() 的本质区别：
 *    Arduino: 一行搞定，内部藏了所有细节
 *    IDF:     需要你自己走完 nvs → netif → event → config → start 链条
 *    好处是每一步都可控，坏处是代码多了 5 倍。但你可以做到 Arduino 做不了的事：
 *      - 强制 11b DSSS + 1Mbps (DBPSK)
 *      - 精确控制 BA Window / AMPDU
 *      - 注册自定义事件回调
 *      - 运行时切换模式
 * ═══════════════════════════════════════════════════════════════
 */
#include "wifi_setup.h"

#include <string.h>

#include "esp_mac.h" /* MACSTR / MAC2STR */
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/ip_addr.h"

/* ──────────── 全局状态 ──────────── */
volatile bool wifi_connected = false;
bool is_ap_mode = false;

/**
 * netif 句柄（网络接口句柄）
 *
 * IDF 中每个网络接口（STA / AP）都有一个 esp_netif_t 对象，
 * 需要保存它以便后续查询 IP 等信息。
 */
static esp_netif_t* s_netif_sta = NULL;
static esp_netif_t* s_netif_ap = NULL;

/**
 * 事件组：用于同步等待 Wi-Fi 连接完成
 *
 * 为什么不用 while 循环 + delay？
 *   IDF 鼓励用 FreeRTOS 事件组来同步，
 *   这样等待期间 CPU 可以去做其他事，不会死循环白费 cycles。
 *   但这里为了简单，用一个 volatile 标志 + vTaskDelay 轮询。
 */
static volatile bool s_got_ip = false;

/* ══════════════════════════════════════════
 *  Wi-Fi 事件回调
 * ══════════════════════════════════════════
 *
 *  IDF 的事件系统：
 *    所有 Wi-Fi 和 TCP/IP 事件都通过一个全局事件循环分发。
 *    你注册回调函数，系统在对应事件发生时调用你。
 *
 *  比 Arduino 好在哪？
 *    Arduino 只有 WiFi.status() 可以轮询，
 *    IDF 可以精准拿到"刚连上/刚断开/拿到IP/AP有人连入"等事件。
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  /* ──── Wi-Fi 事件 ──── */
  if (event_base == WIFI_EVENT) {
    switch (event_id) {

      case WIFI_EVENT_STA_START:
        /* STA 模式启动完成，开始连接 AP */
        ESP_LOGI(TAG_WIFI, "STA 启动，开始连接...");
        esp_wifi_connect();
        break;

      case WIFI_EVENT_STA_CONNECTED: ESP_LOGI(TAG_WIFI, "✅ STA 已连接到 AP"); break;

      case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGW(TAG_WIFI, "❌ STA 断开，尝试重连...");
        wifi_connected = false;
        esp_wifi_connect(); /* 自动重连 */
        break;

      case WIFI_EVENT_AP_STACONNECTED: {
        /* 有设备连入我们的 AP */
        wifi_event_ap_staconnected_t* evt = (wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI(TAG_WIFI, "✅ 设备连入 AP, MAC=" MACSTR, MAC2STR(evt->mac));
        wifi_connected = true;
        break;
      }

      case WIFI_EVENT_AP_STADISCONNECTED: {
        wifi_event_ap_stadisconnected_t* evt = (wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGW(TAG_WIFI, "设备离开 AP, MAC=" MACSTR, MAC2STR(evt->mac));
        /* AP 模式下可能有多个客户端，这里简化处理 */
        wifi_connected = false;
        break;
      }

      default: break;
    }
  }

  /* ──── IP 事件 ──── */
  if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* evt = (ip_event_got_ip_t*)event_data;
      ESP_LOGI(TAG_WIFI, "✅ 获取 IP: " IPSTR, IP2STR(&evt->ip_info.ip));
      wifi_connected = true;
      s_got_ip = true;
    }
  }
}

/* ══════════════════════════════════════════
 *  start_ap — 启动 AP 热点
 * ══════════════════════════════════════════
 *
 *  对应原 Arduino 的 startAPMode()
 *
 *  额外功能：
 *    - 手动设置静态 IP（10.0.0.1）
 *    - 强制 802.11b 协议（最高灵敏度）
 *    - 关闭省电
 *    - 最大发射功率
 */
static void start_ap(void) {
  ESP_LOGI(TAG_WIFI, "启动 AP 模式...");

  /* 创建默认 AP 网络接口（如果还没创建） */
  if (s_netif_ap == NULL) { s_netif_ap = esp_netif_create_default_wifi_ap(); }

  /* ── 设置静态 IP ── */
  /* 必须先停止 DHCP 服务器，才能改 IP */
  esp_netif_dhcps_stop(s_netif_ap);

  esp_netif_ip_info_t ip_info;
  memset(&ip_info, 0, sizeof(ip_info));
  /* 手动解析 IP 地址字符串 → 二进制 */
  ip_info.ip.addr = ipaddr_addr(AP_IP_ADDR);
  ip_info.gw.addr = ipaddr_addr(AP_IP_GW);
  ip_info.netmask.addr = ipaddr_addr(AP_IP_NETMASK);
  esp_netif_set_ip_info(s_netif_ap, &ip_info);

  /* 重新启动 DHCP 服务器（给连入的客户端分配 IP） */
  esp_netif_dhcps_start(s_netif_ap);

  /* ── AP 配置 ── */
  wifi_config_t ap_config = {
      .ap =
          {
              .channel = AP_CHANNEL,
              .max_connection = AP_MAX_CONN,
              .authmode = WIFI_AUTH_WPA2_PSK,
              .pmf_cfg =
                  {
                      .required = false,
                  },
          },
  };
  /* 安全拷贝 SSID 和密码 */
  strncpy((char*)ap_config.ap.ssid, AP_SSID, sizeof(ap_config.ap.ssid));
  ap_config.ap.ssid_len = strlen(AP_SSID);
  strncpy((char*)ap_config.ap.password, AP_PASS, sizeof(ap_config.ap.password));

  // 设置国家码
  wifi_country_t country = {.cc = "JP",   // US 允许 1-11 信道。EU,CN允许 1-13 信道
                            .schan = 1,   //起始信道// JP 允许 1-14 信道 。 01 为世界，允许 1-13 信道
                            .nchan = 14,  //指信道数量，不是最后一个信道// 只有日本允许，且仅限 11b DSSS，但是国内电脑网卡搜不到😭
                            .policy = WIFI_COUNTRY_POLICY_MANUAL};//_MANUAL = 手动 。_AUTO = 跟随 AP（STA 模式下自动适配连接的 AP 的国家码）
  esp_wifi_set_country(&country);


  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  /* ── 802.11b 强制 + 性能优化 ── */

  /**
   * 强制 802.11b 协议
   *
   * WIFI_PROTOCOL_11B = 仅允许 11b 速率 (1/2/5.5/11 Mbps)
   * 11b 使用 DSSS/CCK 调制，比 11g/n 的 OFDM 更抗干扰，
   * 接收灵敏度更高（1Mbps DBPSK ≈ -98dBm）。
   *
   * 代价：最大速率只有 11Mbps，且会拖慢同信道其他 11n 设备。
   * 对 MAVLink（几十 KB/s）完全够用。
   */
  // 1️⃣  1️⃣  1️⃣  1️⃣  1️⃣  1️⃣  1️⃣  1️⃣
  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B);
  esp_wifi_config_80211_tx_rate(WIFI_IF_AP, WIFI_PHY_RATE_1M_L);//配置这个后，其实上面那个设置协议的就没什么意义了，因为速率锁死了，协议自然也就锁死了
  // esp_wifi_internal_set_fix_rate(WIFI_IF_AP, true, WIFI_PHY_RATE_1M_L);//未暴露
#ifdef _TIPS_
  // 因为使用了802.11b （的1Mbps Long Preamble (1M_L)） 而不是 11n，所以没有Long GI
  // 在 esp_wifi_start() 之后调用
  // 强制固定速率
#include "esp_wifi_internal.h"  // 内部 API，不在公开头文件里
  esp_wifi_internal_set_fix_rate(WIFI_IF_AP或WIFI_IF_STA, true, WIFI_PHY_RATE_1M_L);
  // ....or....
  // 不需要include
  esp_wifi_config_80211_tx_rate(WIFI_IF_AP或WIFI_IF_STA, WIFI_PHY_RATE_1M_L);
  //   但这些都是 IDF 内部 API，当前board可能未暴露

  // 如果是ESP-NOW
  esp_now_peer_info_t peer;
  // ... 其他設置 ...
  peer.rate = WIFI_PHY_RATE_1M_L;  // 這裡同樣可以鎖死 1Mbps
  esp_now_add_peer(&peer);
#endif

  /**
   * 关闭省电（P0 必改！）
   *
   * 默认 Modem-sleep 会周期性关闭射频，
   * 导致 ping 抖动和 MAVLink 心跳丢失。
   * 对于嵌入式透传设备，永远关闭省电。
   */
  // 2️⃣  2️⃣  2️⃣  2️⃣  2️⃣  2️⃣  2️⃣  2️⃣
  esp_wifi_set_ps(WIFI_PS_NONE);

  /**
   * 最大发射功率
   *
   * 单位：0.25dBm，84 = 21dBm（ESP32 最大值）
   * 对远距离 MAVLink 链路，永远拉满。
   * 最大80，设置为81 21dbm，会被硬件限制到 ~20.5dBm，不会报错
   */
  // 3️⃣  3️⃣  3️⃣  3️⃣  3️⃣  3️⃣  3️⃣  3️⃣
  esp_wifi_set_max_tx_power(80);

  is_ap_mode = true;
  wifi_connected = false; /* 等待设备连入 */

  ESP_LOGI(TAG_WIFI, "AP 启动完成: SSID=%s, CH=%d, IP=%s, 协议=11b", AP_SSID, AP_CHANNEL, AP_IP_ADDR);
}

/* ══════════════════════════════════════════
 *  try_sta — 尝试 STA 连接
 * ══════════════════════════════════════════
 *
 *  对应原 Arduino 的 connectToSTA()
 *  返回 true = 连接成功并拿到 IP
 *  返回 false = 超时失败，调用者应该回退到 AP 模式
 */
static bool try_sta(void) {
  /* 如果没配 STA SSID，直接跳过 */
  if (strlen(STA_SSID) == 0) {
    ESP_LOGI(TAG_WIFI, "未配置 STA SSID，跳过 STA 模式");
    return false;
  }

  ESP_LOGI(TAG_WIFI, "尝试 STA 连接: %s", STA_SSID);

  /* 创建默认 STA 网络接口 */
  if (s_netif_sta == NULL) { s_netif_sta = esp_netif_create_default_wifi_sta(); }

  wifi_config_t sta_config = {0};
  strncpy((char*)sta_config.sta.ssid, STA_SSID, sizeof(sta_config.sta.ssid));
  strncpy((char*)sta_config.sta.password, STA_PASS, sizeof(sta_config.sta.password));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  /* esp_wifi_start() 触发 WIFI_EVENT_STA_START → 事件回调中调用 esp_wifi_connect() */

  /* 等待获取 IP（带超时） */
  int waited_ms = 0;
  while (!s_got_ip && waited_ms < STA_CONNECT_TIMEOUT_MS) {
    vTaskDelay(pdMS_TO_TICKS(100));
    waited_ms += 100;
  }

  if (s_got_ip) {
    /* STA 连接成功，也强制 11b + 关省电 */
    // 1️⃣  1️⃣  1️⃣  1️⃣  1️⃣  1️⃣  1️⃣  1️⃣
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
    esp_wifi_config_80211_tx_rate(WIFI_IF_STA, WIFI_PHY_RATE_1M_L);//配置这个后，其实上面那个设置协议的就没什么意义了，因为速率锁死了，协议自然也就锁死了
    // esp_wifi_internal_set_fix_rate(WIFI_IF_STA, true, WIFI_PHY_RATE_1M_L);//未暴露
    // 2️⃣  2️⃣  2️⃣  2️⃣  2️⃣  2️⃣  2️⃣  2️⃣
    esp_wifi_set_ps(WIFI_PS_NONE);
    // 3️⃣  3️⃣  3️⃣  3️⃣  3️⃣  3️⃣  3️⃣  3️⃣
    esp_wifi_set_max_tx_power(80);

    is_ap_mode = false;
    ESP_LOGI(TAG_WIFI, "✅ STA 连接成功，协议=11b");
    return true;
  }

  ESP_LOGW(TAG_WIFI, "❌ STA 连接超时 (%d ms)", STA_CONNECT_TIMEOUT_MS);
  /* 清理 STA 状态，后续会切 AP */
  esp_wifi_stop();
  return false;
}

/* ══════════════════════════════════════════
 *  wifi_init — 完整初始化流程
 * ══════════════════════════════════════════
 *
 *  调用链：
 *    app_main()
 *      → wifi_init()
 *          → nvs_flash_init()
 *          → esp_netif_init()
 *          → esp_event_loop_create_default()
 *          → 注册事件回调
 *          → esp_wifi_init()
 *          → try_sta()   // 先尝试 STA
 *          → start_ap()  // STA 失败则开 AP
 */
void wifi_init(void) {
  /* ── 1. NVS Flash 初始化 ──
   *
   * Wi-Fi 驱动内部使用 NVS 存储 RF 校准数据。
   * 如果你不初始化 NVS，esp_wifi_init 会报错。
   */
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    /* NVS 分区满了或版本不匹配 → 擦除重来 */
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  }

  /* ── 2. TCP/IP 协议栈初始化 ──
   *
   * esp_netif_init() 初始化 lwIP 适配层。
   * 必须在 esp_wifi_init() 之前调用。
   */
  ESP_ERROR_CHECK(esp_netif_init());

  /* ── 3. 默认事件循环 ──
   *
   * IDF 的所有异步事件（Wi-Fi 连接/断开、获取 IP 等）
   * 都通过这个事件循环分发。
   */
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  /* ── 4. 注册事件回调 ──
   *
   * ESP_EVENT_ANY_ID = 我对这类事件的所有子事件都感兴趣
   */
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

  /* ── 5. Wi-Fi 驱动初始化 ──
   *
   * WIFI_INIT_CONFIG_DEFAULT() 使用 menuconfig 中的配置值
   * （TX/RX buffer 数量、AMPDU 开关等）
   */
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  /* ── 6. 尝试 STA，失败则 AP ── */
  if (!try_sta()) { start_ap(); }

  ESP_LOGI(TAG_WIFI, "Wi-Fi 初始化完成 (模式=%s)", is_ap_mode ? "AP" : "STA");
}

/* ══════════════════════════════════════════
 *  wifi_check_connection — 主循环中周期检查
 * ══════════════════════════════════════════
 *
 *  对应原 Arduino 的 checkWifiConnection()
 *
 *  AP 模式：检查 esp_wifi_ap_get_sta_list() 是否有客户端
 *  STA 模式：wifi_connected 由事件回调自动更新
 *
 *  注意：这个函数不做重连逻辑，
 *        STA 断开后在事件回调中自动重连。
 */
void wifi_check_connection(void) {
  if (is_ap_mode) {
    wifi_sta_list_t sta_list;
    esp_wifi_ap_get_sta_list(&sta_list);
    wifi_connected = (sta_list.num > 0);
  }
  /* STA 模式下 wifi_connected 已由事件回调维护 */
}

/* ══════════════════════════════════════════
 *  wifi_get_broadcast_addr — 计算广播地址
 * ══════════════════════════════════════════
 *
 *  对应原 Arduino 的 update_broadcast_ip()
 *
 *  AP 模式：固定 10.0.0.255
 *  STA 模式：IP | ~Mask = 广播地址
 */
void wifi_get_broadcast_addr(ip_addr_t* brd_addr) {
  if (is_ap_mode) {
    /* AP 模式：固定广播地址 */
    IP_ADDR4(brd_addr, 10, 0, 0, 255);
  } else {
    /* STA 模式：从当前接口获取 IP 和掩码，计算广播 */
    if (s_netif_sta != NULL) {
      esp_netif_ip_info_t ip_info;
      esp_netif_get_ip_info(s_netif_sta, &ip_info);

      uint32_t ip = ip_info.ip.addr;
      uint32_t mask = ip_info.netmask.addr;
      uint32_t brd = ip | (~mask);

      brd_addr->u_addr.ip4.addr = brd;
      brd_addr->type = IPADDR_TYPE_V4;
    } else {
      /* 理论上不应该走到这里 */
      IP_ADDR4(brd_addr, 255, 255, 255, 255);
    }
  }
}
