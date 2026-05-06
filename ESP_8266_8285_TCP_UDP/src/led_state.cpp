#include "led_state.h"

// ============================================================
//  ESP-01S 只有一个LED: GPIO2 (低电平点亮)
//  GPIO2 同时是 Serial1 TX，有冲突:
//    编译期 _FACTORY_DEBUG → Serial1 用于工厂调试 → LED不可用
//    运行时 DEBUG_ON       → Serial1 用于运行调试 → LED不可用
//    两者都关才能用LED
// ============================================================

unsigned long sequenceLastTime = 0;
unsigned long liteLastTime     = 0;
bool sequenceON   = false;
bool liteON        = false;
bool ledAvailable  = false;

// ----------------------------------------------------------
//  初始化: 判断GPIO2是否可用于LED
// ----------------------------------------------------------
void led_begin() {
#ifdef _FACTORY_DEBUG
    // 编译期就启用了工厂调试 → Serial1 必占GPIO2，LED直接放弃
    ledAvailable = false;
    return;
#else
    // 运行时检查: DebugON → Serial1占GPIO2
    if (DEBUG_ON) {
        ledAvailable = false;
        return;
    }
    ledAvailable = true;
    pinMode(StateIndicator_PIN, OUTPUT);
    digitalWrite(StateIndicator_PIN, HIGH);  // HIGH = 灭 (低电平点亮)
#endif
}


// ----------------------------------------------------------
//  非阻塞闪烁引擎 (从SRC_OLD直接移植)
//  liteONtime:        单次亮灯时间 ms
//  liteOFFtime:       单次灭灯时间 ms
//  blinkTimes:        一组闪烁几次
//  sequenceInterval:  两组之间的间隔 ms
// ----------------------------------------------------------
void blink_method(uint16_t liteONtime, uint16_t liteOFFtime,
                  uint8_t blinkTimes, uint16_t sequenceInterval) {
    static uint8_t blinkNumbers = 0;
    unsigned long nowTime = millis();

    uint16_t liteInterval = liteON ? liteONtime : liteOFFtime;

    if (!sequenceON) {
        // 组间歇期: LED保持灭
        digitalWrite(StateIndicator_PIN, HIGH);   // HIGH = 灭
        if (nowTime - sequenceLastTime >= sequenceInterval) {
            sequenceON = true;
        }
    }

    if (sequenceON) {
        if (nowTime - liteLastTime >= liteInterval) {
            liteLastTime = nowTime;
            liteON = !liteON;
            if (!liteON) {
                blinkNumbers += 1;   // 每次熄灭计数+1
            }
            // 低电平点亮: liteON=true → LOW(亮), liteON=false → HIGH(灭)
            digitalWrite(StateIndicator_PIN, liteON ? LOW : HIGH);
        }
    }

    if (blinkNumbers >= blinkTimes) {
        blinkNumbers = 0;
        liteON = false;
        sequenceON = false;
        liteLastTime = 0;
        sequenceLastTime = nowTime;
    }
}


// ----------------------------------------------------------
//  状态指示 — 在 loop() 中反复调用
// ----------------------------------------------------------
void blink_state() {
    if (!ledAvailable) return;   // Serial1占着GPIO2，啥都别干

    // ---- 已连接: 常亮 ----
    if (deviceConnected) {
        digitalWrite(StateIndicator_PIN, LOW);   // LOW = 亮
        return;
    }

    // ---- 未连接: 按模式闪烁 ----
    if (web_config_mode) {
        blink_method(400, 400, 5, 400);          // Web配置: 快闪5次
    } else {
        switch (CRNT_MODE) {
            case MODE_TCP:
                blink_method(180, 120, 1, 2200);  // TCP: 闪1次
                break;
            case MODE_UDP:
                blink_method(180, 120, 2, 1900);  // UDP: 闪2次
                break;
            default:
                break;
        }
    }
}
