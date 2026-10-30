#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "globals.h"
#include "actuator_control.h"
#include "mqtt_manager.h"

// ============================================================
//  MQTT Connect
// ============================================================
void connectMQTT() {
  Serial.printf("[MQTT] กำลังเชื่อมต่อ %s:%d ...", MQTT_BROKER, MQTT_PORT);

  uint8_t retry = 0;
  while (!mqtt.connected() && retry < 5) {
    bool ok;
    if (strlen(MQTT_USER) > 0) {
      ok = mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS);
    } else {
      ok = mqtt.connect(MQTT_CLIENT_ID);
    }

    if (ok) {
      Serial.println(" ✓");
      // Subscribe คำสั่งจาก Dashboard
      mqtt.subscribe(TOPIC_CMD_LIGHT, 1);
      mqtt.subscribe(TOPIC_CMD_FAN,   1);
      mqtt.subscribe("control/light_mode", 1); // AUTO/MANUAL sync จาก Dashboard
      Serial.println("[MQTT] Subscribed: control/light, control/fan, control/light_mode");
      publishStr(TOPIC_STATUS, "online", true);
    } else {
      Serial.printf(" ✗ (rc=%d) — รอ 3s\n", mqtt.state());
      delay(3000);
      retry++;
    }
  }
}

// ============================================================
//  MQTT Callback — รับคำสั่งจาก Dashboard
// ============================================================
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  char buf[len + 1];
  memcpy(buf, payload, len);
  buf[len] = '\0';
  String msg   = String(buf);
  String t     = String(topic);
  String val   = extractValue(msg);
  bool   turnOn = (val == "ON" || val == "1" || val == "true");

  Serial.printf("[MQTT] ← %s : %s\n", topic, msg.c_str());

  if (t == TOPIC_CMD_LIGHT) {
      // แค่สั่งไฟตามคำสั่งที่ได้รับ — โหมด Auto/Manual ถูกควบคุมแยกผ่าน
      // topic "control/light_mode" เท่านั้น (ไม่แตะ isAutoLightMode ตรงนี้
      // เพราะถ้าตั้ง false ทุกครั้งที่มีคำสั่งเข้ามา จะไปทับค่า AUTO ที่เพิ่ง sync
      // มาจาก Dashboard ตอนที่ระบบ Auto บนเว็บสั่งไฟผ่าน topic เดียวกันนี้)
      setLight(turnOn);
  } else if (t == TOPIC_CMD_FAN) {
      // เดิม topic นี้ subscribe ไว้แต่ไม่เคยถูกจัดการ ทำให้ปุ่มพัดลมบน
      // Dashboard ไม่มีผลกับบอร์ดเลย เพิ่ม handler ให้ครบ
      setFan(turnOn);
  } else if (t == "control/light_mode") {
      // Dashboard บอกโหมดปัจจุบันมาให้: AUTO = ให้บอร์ดกลับไปใช้เซนเซอร์ lux
      // ควบคุมไฟเอง, MANUAL = ปิดระบบ auto บนบอร์ด รอรับคำสั่งจาก control/light
      isAutoLightMode = (val == "AUTO");
      Serial.printf("[MQTT] Light mode → %s\n", isAutoLightMode ? "AUTO" : "MANUAL");
  }
}

// ============================================================
//  Helpers
// ============================================================

// ส่งค่า float เป็น JSON {"value": X}
void publishFloat(const char* topic, float value, uint8_t decimals) {
  char payload[48];
  if (decimals == 0)      snprintf(payload, sizeof(payload), "{\"value\":%.0f}", value);
  else if (decimals == 1) snprintf(payload, sizeof(payload), "{\"value\":%.1f}", value);
  else                    snprintf(payload, sizeof(payload), "{\"value\":%.2f}", value);
  mqtt.publish(topic, payload, false);
}

// ส่งค่า string เป็น JSON {"value": "X"}
void publishStr(const char* topic, const char* value, bool retain) {
  char payload[64];
  snprintf(payload, sizeof(payload), "{\"value\":\"%s\"}", value);
  mqtt.publish(topic, payload, retain);
}

// ดึง value จาก JSON string {"value":"..."}
String extractValue(const String& json) {
  StaticJsonDocument<128> doc;
  if (!deserializeJson(doc, json) && doc.containsKey("value")) {
    return doc["value"].as<String>();
  }
  // Fallback: plain text เช่น "ON", "OFF"
  String s = json;
  s.trim();
  return s;
}
