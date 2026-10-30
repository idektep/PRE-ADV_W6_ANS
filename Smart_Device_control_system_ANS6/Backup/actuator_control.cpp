#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "mqtt_manager.h"
#include "actuator_control.h"

// ── ควบคุมไฟห้อง ──
void setLight(bool on) {
  ledLightOn = on;
  digitalWrite(PIN_LED_LIGHT, on ? HIGH : LOW);
  publishStr("sensors/device/light_state", on ? "ON" : "OFF");
  Serial.printf("[CTRL] 💡 ไฟห้อง → %s\n", on ? "ON" : "OFF");
}

// ── ควบคุมพัดลม ──
void setFan(bool on) {
  ledFanOn = on;
  digitalWrite(PIN_LED_FAN, on ? LOW : HIGH);
  publishStr("sensors/device/fan_state", on ? "ON" : "OFF");
  Serial.printf("[CTRL] 🌀 พัดลม  → %s\n", on ? "ON" : "OFF");
}
