#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "wifi_manager.h"

// ============================================================
//  WiFi
// ============================================================
void connectWiFi() {
  Serial.printf("[WiFi] กำลังเชื่อมต่อ \"%s\"", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint8_t retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 40) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] ✓ Connected  IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] ✗ Failed — Restarting...");
    delay(1000);
    ESP.restart();
  }
}
