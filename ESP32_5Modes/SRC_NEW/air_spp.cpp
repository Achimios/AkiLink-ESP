#include "air_spp.h"

// 引用库可以都放在.cpp里，.h只放对外接口和全局变量声明
#include "esp_spp_api.h" // 替换 BluetoothSerial.h
#include "data_config.h"
#include "buf_rules.h"
// ===== 补上缺失的头文件 =====
#include "esp_bt.h"
#include "esp_bt_main.h"
// ==========================

// 全局变量，用于保存SPP连接状态和句柄
static uint32_t spp_handle = 0;
static bool spp_congested = false; // 全局拥塞标志

// 核心：SPP事件回调函数
static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
  // ... (回调函数的代码保持不变，此处省略)
  switch (event)
  {
  case ESP_SPP_INIT_EVT:
    DEBUG_PRINTLN("SPP Init, starting server...");
    esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, 0, ESP_SPP_NAME);
    break;

  case ESP_SPP_SRV_OPEN_EVT: // 客户端已连接
    DEBUG_PRINTLN("SPP Client Connected.");
    deviceConnected = true;
    spp_handle = param->srv_open.handle; // 保存连接句柄
    break;
  // case ESP_SPP_OPEN_EVT: // 客户端模式（如果你的 ESP32 主动连别人）
  //   break;

  case ESP_SPP_CLOSE_EVT: // 客户端断开
    DEBUG_PRINTLN("SPP Client Disconnected.");
    deviceConnected = false;
    spp_handle = 0;
    w2aLen = 0; // 清空发送缓冲区
    break;

  case ESP_SPP_DATA_IND_EVT: // 接收到数据事件
  {
    //*1️1️1️1️== airRX → a2w环形Buf (零拷贝) ==1️1️1️1️*
    packetSize = (int16_t)param->data_ind.len; //(int16_t)显示转换保险一点，虽然SPP长度不可能超过32767，但万一有bug导致溢出，变成负数就麻烦了。总之SPP包大小不可能超过环形缓冲区剩余空间，所以不会有安全问题。
    int16_t first_part = checkRingSpace(packetSize);
    if (first_part >= 0)
    {
      memcpy(a2wRing + a2wHead, param->data_ind.data, first_part);
      if (a2wToWrite > first_part)
      {
        memcpy(a2wRing, param->data_ind.data + first_part, a2wToWrite - first_part);
      }
      a2wHead = (a2wHead + a2wToWrite) & A2W_RING_SIZE_SUB_ONE;
      DEBUG_PRINTLN(ringLack > 0 ? "填满，缺少：" + String(ringLack) + " B" : ringLack == 0 ? "正好填满"
                                                                                            : "填入，剩余：" + String(-ringLack) + " B");
    }
    else
    {
      DEBUG_PRINTLN("填入前已满，缺少：" + String(ringLack) + " Byte");
    }
    break;
  }
  case ESP_SPP_CONG_EVT:
    spp_congested = param->cong.cong;
    if (spp_congested)
      DEBUG_PRINTLN("[SPP] 缓冲区满，暂停发送");
    else
      DEBUG_PRINTLN("[SPP] 缓冲区已清空，恢复发送");
    break;

  case ESP_SPP_WRITE_EVT:
    break;

  case ESP_SPP_START_EVT:
    DEBUG_PRINTLN("SPP Server started.");
    break;
  default:
    break;
  }
}

void airSpp_begin()
{
  // 1. 释放和关闭当前可能在运行的蓝牙模式 (非常重要！)
  // 如果之前有任何蓝牙活动（特别是 NimBLE），需要先 deinit
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED)
  {
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED)
  {
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
  }

  // 2. 初始化蓝牙控制器并启用 Classic BT 模式
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  if (esp_bt_controller_init(&bt_cfg) != ESP_OK)
  {
    DEBUG_PRINTLN("Initialize controller failed");
    return;
  }
  if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK)
  {
    DEBUG_PRINTLN("Enable controller failed");
    return;
  }

  // 3. 初始化 Bluedroid 协议栈
  if (esp_bluedroid_init() != ESP_OK || esp_bluedroid_enable() != ESP_OK)
  {
    DEBUG_PRINTLN("Initialize bluedroid failed");
    return;
  }

  // 4. 注册我们的SPP回调函数
  if (esp_spp_register_callback(esp_spp_cb) != ESP_OK)
  {
    DEBUG_PRINTLN("SPP register callback failed");
    return;
  }

  // 5. 初始化SPP模块为回调模式
  if (esp_spp_init(ESP_SPP_MODE_CB) != ESP_OK)
  {
    DEBUG_PRINTLN("SPP init failed");
    return;
  }
  DEBUG_PRINTLN("ESP32 SPP Bridge (Callback Mode) Initialized");
}

void airSpp_update()
{
  // ... (update 函数的代码保持不变，此处省略)
  //*2️2️2️2️== a2w环形Buf → SerialTX ==2️2️2️2️*
  RingToSerial();

  if (!deviceConnected)
    return;

  //*3️3️3️3️== SerialRX → w2a线性Buf ==3️3️3️3️*
  serialToW2aBuf();

  if (spp_congested)
    return; // 如果SPP缓冲区满了，先不发送，等下次update再试

  //*4️4️4️4️== w2a线性Buf → AirTX    ==4️4️4️4️*
  if (shouldFlush())
  {
    esp_err_t result = esp_spp_write(spp_handle, w2aLen, w2aBuf);
    if (result == ESP_OK)
      w2aLen = 0;
    else
      DEBUG_PRINTLN("SPP write failed, congested. Retrying next cycle.");
  }
}
