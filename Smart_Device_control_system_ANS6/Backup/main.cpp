/*
 ============================================================
  ESP32 IoT Dashboard — PlatformIO / VSCode
 ============================================================
  Board   : ESP32 Dev Module
  Broker  : EMQX (broker.emqx.io)

  Sensors:
    DHT22       Temp + Humidity     → GPIO 23
    BH1750      Light (I2C)         → SDA 21 / SCL 22
    MQ135       CO2  (Analog)       → GPIO 34
    Soil        Moisture (Analog)   → GPIO 35
    Rain        Intensity (Analog)  → GPIO 35
    Rain        Detected  (Digital) → GPIO 5

  Output:
    LED/Relay ไฟห้อง → GPIO 16
    LED/Relay พัดลม  → GPIO 27

  การตั้งค่า: แก้ไขที่ include/config.h
 ============================================================
  โครงสร้างไฟล์ (แยกตามการทำงานของแต่ละเซนเซอร์/โมดูล เพื่อสอนทีละสัปดาห์):
    include/globals.h          → ตัวแปร/อ็อบเจกต์ที่ใช้ร่วมกันทุกไฟล์
    src/main.cpp                → setup() / loop() หลัก + readAndPublish()
    src/wifi_manager.*           → เชื่อมต่อ WiFi
    src/mqtt_manager.*           → เชื่อมต่อ MQTT, callback, publish/parse helper
    src/sensor_dht.*             → DHT22 (Temperature + Humidity)
    src/sensor_light.*           → BH1750 (Light) + Auto Light Control
    src/sensor_co2.*             → MQ135 (CO2)
    src/sensor_soil.*            → Soil Moisture
    src/sensor_rain.*            → Rain Sensor + Flood Risk Alert
    src/actuator_control.*       → ควบคุมไฟห้อง / พัดลม (relay)
 ============================================================
*/

#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "sensor_dht.h"
#include "sensor_light.h"
#include "sensor_co2.h"
#include "sensor_soil.h"
#include "sensor_rain.h"
#include "actuator_control.h"

// ─────────────────────────────────────────
//  Objects
// ─────────────────────────────────────────
DHT          dht(PIN_DHT, DHT_TYPE);
BH1750       lightMeter;
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

// ─────────────────────────────────────────
//  State
// ─────────────────────────────────────────
unsigned long lastPublish      = 0;
bool          ledLightOn       = false;
bool          ledFanOn         = false;
bool          isAutoLightMode  = true;

// ============================================================
//  Prototypes
// ============================================================
void readAndPublish();

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n╔══════════════════════════════╗");
  Serial.println("║  ESP32 IoT Dashboard v2.0    ║");
  Serial.println("╚══════════════════════════════╝");

  // ── GPIO ──
  pinMode(PIN_LED_LIGHT, OUTPUT);
  pinMode(PIN_LED_FAN,   OUTPUT);
  pinMode(PIN_RAIN_DO,   INPUT);
  digitalWrite(PIN_LED_LIGHT, LOW);
  digitalWrite(PIN_LED_FAN,   HIGH);

  // ── I2C ──
  Wire.begin(PIN_SDA, PIN_SCL);

  // ── DHT22 ──
  dht.begin();
  Serial.println("[INIT] DHT22 OK");

  // ── BH1750 ──
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("[INIT] BH1750 OK");
  } else {
    Serial.println("[INIT] BH1750 FAIL — ตรวจสอบสาย SDA/SCL");
  }

  // ── WiFi ──
  connectWiFi();

  // ── MQTT ──
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setKeepAlive(60);
  connectMQTT();

  Serial.println("[INIT] พร้อมทำงาน ✓\n");
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();

  unsigned long now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;
    readAndPublish();
  }
}

// ============================================================
//  อ่านเซ็นเซอร์และส่ง MQTT (เรียกใช้ฟังก์ชันของแต่ละโมดูลที่แยกไว้)
// ============================================================
void readAndPublish() {
  Serial.println("┌─── Read Sensors ───────────────");

  readDHTAndPublish();
  readLightAndPublish();
  readCO2AndPublish();

  float soilPct = readSoilAndPublish();
  bool  isRain  = readRainAndPublish();

  // ── Flood Risk Alert ──
  if (isRain && soilPct > 150.0f) {
    publishStr(TOPIC_FLOOD_ALERT, "HIGH");
    Serial.println("│ [ALERT]  ⚠️  ฝนตก + ดินชื้นสูง — เสี่ยงน้ำท่วม!");
  }

  Serial.println("└────────────────────────────────\n");
}
