#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "mqtt_manager.h"
#include "sensor_dht.h"

// ============================================================
//  DHT22: Temperature & Humidity
// ============================================================
void readDHTAndPublish() {
  // ── DHT22: Temperature & Humidity ──
  float temp  = dht.readTemperature();
  float humid = dht.readHumidity();

  if (!isnan(temp) && !isnan(humid)) {
    publishFloat(TOPIC_TEMP,  temp,  1);
    publishFloat(TOPIC_HUMID, humid, 1);
    Serial.printf("│ [DHT22]  Temp=%.1f°C  Humid=%.1f%%\n", temp, humid);
  } else {
    Serial.println("│ [DHT22]  ✗ Read ERROR");
  }
}
