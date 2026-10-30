#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

//===============================================================//
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>




#include <ArduinoJson.h>
#include "config.h"

//=================== Servo =======================================//

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// สร้าง Object PCA9685
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// ตั้งค่า Pulse ของ Servo (ค่ามาตรฐานคือ 150-600)
#define SERVOMIN  150 
#define SERVOMAX  600 
#define SERVO_FREQ 50 

// --- กำหนดช่องสัญญาณ PCA9685 

#define SERVO_ROTATE_CHANNEL 5  


//==========================================================//



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
unsigned long lastPublish  = 0;
bool          ledLightOn   = false;
bool          ledFanOn     = false;
bool          isAutoLightMode = false;  
float         lightThreshold  = 10.0f;  
char          lightCompareOp  = '<';    
bool          isAutoFanMode   = false;  
float         fanThreshold    = 30.0f;  

// ============================================================
//  ── Prototypes ──
// ============================================================
void connectWiFi();
void connectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int len);
void readAndPublish();
void publishFloat(const char* topic, float value, uint8_t decimals);
void publishStr(const char* topic, const char* value, bool retain = false);
String extractValue(const String& json);
void setLight(bool on);
void setFan(bool on);
void checkAutoLight(int lux);
void checkAutoFan(float temp);

void Servo_rotate (float pos);
void checkRainWiper(bool isRaining);

//=========================== Servo ===================================//

// ฟังก์ชันแปลงองศาเป็น Pulse
int degreeToPulse(int angle) {
  return map(angle, 0, 180, SERVOMIN, SERVOMAX);
}


//==================================================================//

//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n╔══════════════════════════════╗");
  Serial.println("║  ESP32 IoT Dashboard v2.0    ║");
  Serial.println("╚══════════════════════════════╝");


  // ====== Setup PCA9685 Servo ====================
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);  
  delay(10);


  //================================================


  // ── GPIO ──

  



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
      mqtt.subscribe("control/light_mode", 1); 
      mqtt.subscribe("control/light_threshold", 1); 
      mqtt.subscribe("control/light_op", 1); 
      mqtt.subscribe("control/fan_mode", 1); 
      mqtt.subscribe("control/fan_threshold", 1); 
      Serial.println("[MQTT] Subscribed: control/light, control/fan, control/light_mode, control/light_threshold, control/light_op, control/fan_mode, control/fan_threshold");
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

      if (!isAutoLightMode) {
        setLight(turnOn);
      }
  } else if (t == TOPIC_CMD_FAN) {

      if (!isAutoFanMode) {
        setFan(turnOn);
      }
  } else if (t == "control/fan_mode") {
      isAutoFanMode = (val == "AUTO");
      Serial.printf("[MQTT] Fan mode → %s\n", isAutoFanMode ? "AUTO" : "MANUAL");
  } else if (t == "control/fan_threshold") {

      float newFanThreshold = val.toFloat();
      if (newFanThreshold > 0) {
        fanThreshold = newFanThreshold;
        Serial.printf("[MQTT] Fan threshold → temp >= %.1f\n", fanThreshold);
      }
  } else if (t == "control/light_mode") {
 
    isAutoLightMode = (val == "AUTO");
      Serial.printf("[MQTT] Light mode → %s\n", isAutoLightMode ? "AUTO" : "MANUAL");
  } else if (t == "control/light_threshold") {

      float newThreshold = val.toFloat();
      if (newThreshold > 0) {
        lightThreshold = newThreshold;
        Serial.printf("[MQTT] Light threshold → lux < %.1f\n", lightThreshold);
      }
  } else if (t == "control/light_op") {
  
      if (val == "<" || val == ">") {
        lightCompareOp = val[0];
        Serial.printf("[MQTT] Light compare op → lux %c %.1f\n", lightCompareOp, lightThreshold);
      }
  }
}

// ── ควบคุมไฟ เมือห้องมืด ──
void checkAutoLight(int lux) {
  if (isAutoLightMode) { // ถ้ากดแมนนวลบรรทัดนี้ false
    bool shouldTurnOn = (lightCompareOp == '<') ? (lux < lightThreshold) : (lux > lightThreshold);
    setLight(shouldTurnOn);
  }
}

void checkAutoFan(float temp) {
  if (isAutoFanMode) { // ถ้ากดแมนนวลบรรทัดนี้ false
    if (temp >= fanThreshold && !ledFanOn) {
      setFan(true);
    } else if (temp < fanThreshold - 2.0f && ledFanOn) {
      setFan(false);
    }
  }
}



// ============================================================
//  อ่านเซ็นเซอร์และส่ง MQTT
// ============================================================
void readAndPublish() {
  Serial.println("┌─── Read Sensors ───────────────");

  // ── DHT22: Temperature & Humidity ──
  float temp  = dht.readTemperature();
  float humid = dht.readHumidity();

  if (!isnan(temp) && !isnan(humid)) {
    publishFloat(TOPIC_TEMP,  temp,  1);
    publishFloat(TOPIC_HUMID, humid, 1);
    Serial.printf("│ [DHT22]  Temp=%.1f°C  Humid=%.1f%%\n", temp, humid);
    checkAutoFan(temp);
  } else {
    Serial.println("│ [DHT22]  ✗ Read ERROR");
  }






  // ── BH1750: Light ──
  float lux = lightMeter.readLightLevel();
  if (lux >= 0) {
    publishFloat(TOPIC_LIGHT, lux, 0);
    Serial.printf("│ [BH1750] Light=%.0f lux\n", lux);
    checkAutoLight(lux); 
  } else {
    Serial.println("│ [BH1750] ✗ Read ERROR");
  }






  // ── MQ135: CO2 ──
  





  // ── Soil Moisture ──
 








  // ── Rain Sensor ──
  






  // ── Flood Risk Alert ──
  





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



// ── ควบคุมไฟห้อง ──
void setLight(bool on) {
  
  //.....................................
  //.....................................

  publishStr("sensors/device/light_state", on ? "ON" : "OFF");
  Serial.printf("[CTRL] 💡 ไฟห้อง → %s\n", on ? "ON" : "OFF");
}

// ── ควบคุมพัดลม ──
void setFan(bool on) {
  //.....................................
  //....................................

  publishStr("sensors/device/fan_state", on ? "ON" : "OFF");
  Serial.printf("[CTRL] 🌀 พัดลม  → %s\n", on ? "ON" : "OFF");
}


void Servo_rotate (float pos)
{
  int pluse = degreeToPulse((int)pos);
  pwm.setPWM(SERVO_ROTATE_CHANNEL, 0, pluse);
}



// ── ควบคุม Servo ปัดน้ำฝน: เมื่อฝนตกให้หมุนสลับ 0°↔180° คล้ายที่ปัดน้ำฝนรถยนต์ ==================================//

void checkRainWiper(bool isRaining) {
  const unsigned long WIPER_STEP_MS = 700;   // ความเร็วในการปัด (ปรับได้)
  static bool          wiperAtMax   = false; // false = อยู่ที่ 0°, true = อยู่ที่ 180°
  static unsigned long lastStepTime = 0;
  static bool          wiperParked  = true;  // true = จอดนิ่งอยู่ที่ 0° แล้ว ไม่ต้องสั่งซ้ำ


  if (isRaining) {
    wiperParked = false;
    unsigned long now = millis();
    if (now - lastStepTime >= WIPER_STEP_MS) {
      lastStepTime = now;
      wiperAtMax = !wiperAtMax;
      Servo_rotate(wiperAtMax ? 180.0f : 0.0f);
      Serial.printf("[SERVO] 🌧️  ปัดน้ำฝน → %d°\n", wiperAtMax ? 180 : 0);
    }
  } else if (!wiperParked) {
    // ฝนหยุดแล้ว สั่งกลับไปพักที่ 0° ครั้งเดียว
    Servo_rotate(0);
    wiperAtMax  = false;
    wiperParked = true;
    Serial.println("[SERVO] ☀️  ฝนหยุด — พักใบปัดที่ 0°");
  }
}