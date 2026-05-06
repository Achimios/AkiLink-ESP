#include "modules_run.h"

#include "air_tcp.h"
#include "air_udp.h"
#include "led_state.h"
#include "web_tune.h"
#include "buf_rules.h"

// ============================================================
//  ESP8266 / ESP8285  modules_run.cpp
//  条件编译宏 — 哪个模块没include，对应 BEGIN/UPDATE 就为空
// ============================================================

#ifdef WEB_TUNE_H
#define WEB_TUNE_BEGIN()  webConfig.begin()
#define WEB_TUNE_UPDATE() webConfig.update()
#else
#define WEB_TUNE_BEGIN()
#define WEB_TUNE_UPDATE()
#endif

#ifdef AIR_TCP_H
#define AIR_TCP_BEGIN()  airTcp_begin()
#define AIR_TCP_UPDATE() airTcp_update()
#else
#define AIR_TCP_BEGIN()
#define AIR_TCP_UPDATE()
#endif

#ifdef AIR_UDP_H
#define AIR_UDP_BEGIN()  airUdp_begin()
#define AIR_UDP_UPDATE() airUdp_update()
#else
#define AIR_UDP_BEGIN()
#define AIR_UDP_UPDATE()
#endif


// ============================================================
//  RTC 内存 — 跨软重启保存 web_config 标志
// ============================================================
//  ESP8266 RTC User Memory: 512字节 (128个uint32_t slot)
//  slot 0 用于 web_config 标志
//  ESP.rtcUserMemoryRead/Write(offset, &data, sizeof) offset=4字节块号
#define RTC_SLOT_WEBCFG    0
#define RTC_MAGIC_WEBCFG   0x57454231   // "WEB1" → 进入web_config
// 无 magic 或其他值 → 正常模式

static unsigned long btnPressStart = 0;
static bool btnHeld = false;
#define BTN_HOLD_MS 500    // 长按 0.5 秒触发


// ============================================================
//  读/写 RTC web_config 标志
// ============================================================
static bool readRtcWebFlag() {
    uint32_t val = 0;
    ESP.rtcUserMemoryRead(RTC_SLOT_WEBCFG, &val, sizeof(val));
    return (val == RTC_MAGIC_WEBCFG);
}

static void writeRtcWebFlag(bool enter) {
    uint32_t val = enter ? RTC_MAGIC_WEBCFG : 0;
    ESP.rtcUserMemoryWrite(RTC_SLOT_WEBCFG, &val, sizeof(val));
}


// ============================================================
//  按钮初始化
// ============================================================
// ESP-01S GPIO0: 内部上拉，普通按钮（非自锁！）
//   松开=HIGH，按下=LOW
//   ⚠️ GPIO0 开机必须HIGH，否则进入下载模式
//   所以上电时按钮一定松开 — 用 RTC 标志判断是否进 web_config
void ButtonsInit() {
    led_begin();

    pinMode(CFG_PIN, INPUT);  // GPIO0 自带上拉

    // 检查 RTC 标志 → 是否从"长按→重启"进来的
    if (readRtcWebFlag()) {
        web_config_mode = true;
        writeRtcWebFlag(false);   // 立即清除，防止意外循环
        _FACTORY_DEBUG_PRINTLN("[BTN] RTC flag → web_config_mode");
    } else {
        web_config_mode = false;
    }
}


// ============================================================
//  按钮轮询 — 长按0.5秒 → 亮灯 → 松手后0.5秒重启
// ============================================================
//  流程:
//    1. 按住 ≥0.5s → 强制点亮LED (不管Serial1占没占GPIO2)
//    2. 继续按住 → LED保持常亮
//    3. 松手 → 写 RTC 标志 → 延迟 0.5s → ESP.restart()
//    4. 重启后 GPIO0=HIGH (已松开), RTC读到标志 → web_config_mode
//
//  正常模式 长按 → 写magic → 重启 → 进入web_config
//  web_config 长按 → 不写magic → 重启 → 正常模式
void ButtonsUpdate() {
    bool pressed = (digitalRead(CFG_PIN) == LOW);

    if (pressed && !btnHeld) {
        if (btnPressStart == 0) {
            btnPressStart = millis();
        }
        if (millis() - btnPressStart >= BTN_HOLD_MS) {
            btnHeld = true;
            // 强制点亮LED — 即使Serial1占着GPIO2也无所谓，反正要重启了
            pinMode(StateIndicator_PIN, OUTPUT);
            digitalWrite(StateIndicator_PIN, LOW);   // LOW = 亮
            _FACTORY_DEBUG_PRINTLN("[BTN] 长按0.5s → LED亮，等待松手...");
        }
    }

    if (btnHeld && pressed) {
        // 持续按住 → LED保持亮 (什么都不做，等松手)
    }

    if (btnHeld && !pressed) {
        // 松手了！
        _FACTORY_DEBUG_PRINTLN("[BTN] 松手 → 准备重启");

        if (!web_config_mode) {
            // 正常模式 → 重启进 web_config
            writeRtcWebFlag(true);
        } else {
            // web_config → 重启回正常模式 (不写magic)
            writeRtcWebFlag(false);
        }

        delay(500);   // 松手后0.5秒再重启，稳稳的
        ESP.restart();
    }

    if (!pressed && !btnHeld) {
        btnPressStart = 0;
    }
}


// ============================================================
//  模块初始化 — setup() 中调用
// ============================================================
void modulesInit() {
#ifdef _FACTORY_DEBUG
    debugConfigCheck();  // 打印当前LittleFS存储内容
#endif

    checkStoredConfig();  // LittleFS 配置校验 + 读取
    init_s_debug();       // 条件性初始化 Serial1 (看 _FACTORY_DEBUG / DEBUG_ON)
    init_s_data();        // 初始化 Serial (UART0) 数据口

    // ESP8266 串口固定映射，不存在 ESP32 那个 GPIO 矩阵冲突问题

    if (DEBUG_ON) {
        delay(50);
        DEBUG_PRINTLN("ClapTrap's Coming 111");
        delay(50);
        DEBUG_PRINTLN("ClapTrap's Coming 222");
        delay(50);
        DEBUG_PRINTLN("ClapTrap's Coming 333");
        delay(50);
        DEBUG_PRINTLN("BOOT SUCCESS");
    }

    // RTC标志 → 可能是 web_config_mode
    if (web_config_mode) {
        WEB_TUNE_BEGIN();
    } else {
        getMaxPayLoad();
        switch (CRNT_MODE) {
            case MODE_TCP: AIR_TCP_BEGIN(); break;
            case MODE_UDP: AIR_UDP_BEGIN(); break;
            default: break;
        }
    }
}


// ============================================================
//  模块轮询 — loop() 中调用
// ============================================================
void modulesUpdate() {
    // ---- 连接状态变化处理 ----
    if (deviceConnected && !oldDeviceConnected) {
        // 刚连上的那一刻 → 清空所有缓冲区
        w2aLen = 0;
        a2wTail = a2wHead;       // ring buf 归零 (head不动, tail追上=空)
        a2wToWrite = 0;
        packetSize = 0;
        ringLack = 0;
        lastFlushUs = micros();

        DEBUG_PRINTLN(web_config_mode
            ? "连接，模式=网页调参"
            : ("连接，模式=" + String(CRNT_MODE)));
        oldDeviceConnected = true;

    } else if (!deviceConnected && oldDeviceConnected) {
        // 刚断开的那一刻
        oldDeviceConnected = false;
    }

    // ---- 主业务轮询 ----
    if (web_config_mode) {
        WEB_TUNE_UPDATE();
    } else {
        switch (CRNT_MODE) {
            case MODE_TCP: AIR_TCP_UPDATE(); break;
            case MODE_UDP: AIR_UDP_UPDATE(); break;
            default: break;
        }
    }

    // ---- LED 状态指示 (非vTask，直接loop调用) ----
    blink_state();
}
