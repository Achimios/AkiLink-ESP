#include <WiFi.h>
#include <WiFiUdp.h>

// ================= WiFi =================
const char* AP_SSID = "ESP32_UDP_透传_test_ino12";
const char* AP_PASS = "11111111";

IPAddress localIP(10, 0, 0, 1);
IPAddress gateway(10, 0, 0, 1);
IPAddress subnet(255, 255, 255, 0);

// ================= UDP =================
WiFiUDP udp_t;
const uint16_t ESP32_UDP_PORT = 5600;
const uint16_t PC_UDP_PORT = 5700;

// ================= Remote PC =================
IPAddress pcIP = IPAddress(10, 0, 0, 255);
bool pcKnown = false;

// ================= Serial buffer =================
static uint8_t serBuf[256];
static size_t serLen = 0;
static uint32_t lastSendMs = 0;

#define S_DATA Serial
// =================================================
void setup() {
  Serial.begin(115200);
  S_DATA.begin(115200);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(localIP, gateway, subnet);
  WiFi.softAP(AP_SSID, AP_PASS);

  udp_t.begin(ESP32_UDP_PORT);

  Serial.println("=== ESP32 UDP DEBUG BRIDGE ===");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("Type in Serial Monitor to send UDP");
}

// =================================================
void loop() {
  // --------- UDP -> Serial ----------
  int packetSize = udp_t.parsePacket();
  if (packetSize > 0) {
    uint8_t buf[512];
    int len = udp_t.read(buf, sizeof(buf));

    if (len > 0) {
      if (!pcKnown) {
        pcIP = udp_t.remoteIP();
        Serial.println("[UDP RX] Packet from " + pcIP.toString() + ":" +
                       String(udp_t.remotePort()));
        pcKnown = true;
      }

      Serial.print("[UDP RX] " + String(len) + " bytes from " +
                   pcIP.toString() + ": ");
      S_DATA.write(buf, len);
      Serial.println();
    }
  }

  // --------- Serial -> UDP ----------
  while (S_DATA.available() && serLen < sizeof(serBuf)) {
    serBuf[serLen++] = S_DATA.read();
  }

  // 发送条件：有数据 且 (满了 / 1ms 超时)
  if (serLen > 0 && (serLen >= 32 || millis() - lastSendMs >= 3)) {
    // if (pcKnown) {
    udp_t.beginPacket(pcIP, PC_UDP_PORT);
    udp_t.write(serBuf, serLen);
    udp_t.endPacket();
    // }

    serLen = 0;
    lastSendMs = millis();
  }

  yield();
}
