#include "led_state.h"

unsigned long sequenceLastTime = 0;
unsigned long liteLastTime = 0;
bool sequenceON = false;
bool liteON = false;

void blink_begin() {
  pinMode(AirReadyIndicator_PIN, OUTPUT);
  digitalWrite(AirReadyIndicator_PIN, LOW);
  pinMode(StateIndicator_PIN, OUTPUT);
  digitalWrite(StateIndicator_PIN, LOW);
}


void blink_method(uint16_t liteONtime, uint16_t liteOFFtime, uint8_t blinkTimes, uint16_t sequenceInterval) {
  static uint8_t blinkNumbers = 0;
  unsigned long nowTime = millis();  // ⏰️
  uint16_t liteInterval = liteON ? liteONtime : liteOFFtime;
  if (!sequenceON) {
    digitalWrite(StateIndicator_PIN, LOW);  // 不检测现在是否已经LOW，反正持续写LOW，应该没事吧？
    if (nowTime - sequenceLastTime >= sequenceInterval) { sequenceON = true; }
  }
  if (sequenceON) {
    if (nowTime - liteLastTime >= liteInterval) {
      liteLastTime = nowTime;  // ⏰️
      liteON = !liteON;
      if (!liteON) {  // 每次熄灭就+1
        blinkNumbers += 1;
      }  // if bool
      digitalWrite(StateIndicator_PIN, liteON);
    }  // if >=
  }  // if bool
  if (blinkNumbers >= blinkTimes) {  // 最后一次熄灭时，结束本sequence
    blinkNumbers = 0;
    liteON = false;  // lite为灭，
    sequenceON = false;
    liteLastTime = 0;  // ⏰️
    // 注意俩sequence之间的亮灯间隔不是liteOFF time +
    // sequenceInterval，也不是看谁更大选谁，而是单独的sequenceInterval，这样更灵活
    sequenceLastTime = nowTime;  // ⏰️
  }  // if >=
}  // void


void blink_state() {  // 2个灯，StateIndicator作为连接和模式指示LED，AirReadyIndicator则是给上位设备的指示信号
  if (deviceConnected) {
    digitalWrite(StateIndicator_PIN, HIGH);  // wifi连接了就亮，spp和ble客户端连接了就亮
    if (CRNT_MODE != MODE_TCP) {
      digitalWrite(AirReadyIndicator_PIN, HIGH);  // UDP,spp,ble只有1级连接
    } else {
      if (tcpClientConnected)
        digitalWrite(AirReadyIndicator_PIN, HIGH);  // TCP模式下，只有真正有客户端连接时AirReady才常亮
      else
        digitalWrite(AirReadyIndicator_PIN, LOW);
    }
    goto end;  // 连接时不执行后续闪烁逻辑
  }
  digitalWrite(AirReadyIndicator_PIN, LOW);  // 没连接就关闭
  if (web_config_mode) {
    blink_method(400, 400, 5, 400);
  } else {
    switch (CRNT_MODE) {
      case MODE_TCP:
        blink_method(180, 120, MODE_TCP, 2200);  // MODE_TCP等是1~5的 enum
        break;
      case MODE_UDP: blink_method(180, 120, MODE_UDP, 1900); break;
      case MODE_SPP: blink_method(180, 120, MODE_SPP, 1600); break;
      case MODE_BLE: blink_method(180, 120, MODE_BLE, 1300); break;
      case MODE_ESPNOW: blink_method(180, 120, MODE_ESPNOW, 1000); break;
      default: break;
    }  // switch
  }  // else
end:;
}  // void