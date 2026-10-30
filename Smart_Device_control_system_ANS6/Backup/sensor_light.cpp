#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "mqtt_manager.h"
#include "actuator_control.h"
#include "sensor_light.h"

// ============================================================
//  BH1750: Light
// ============================================================
void readLightAndPublish() {
  // ── BH1750: Light ──
  float lux = lightMeter.readLightLevel();
  if (lux >= 0) {
    publishFloat(TOPIC_LIGHT, lux, 0);
    Serial.printf("│ [BH1750] Light=%.0f lux\n", lux);
    checkAutoLight(lux); 
  } else {
    Serial.println("│ [BH1750] ✗ Read ERROR");
  }
}

// ── ควบคุมพัดลม เมือห้องมืด ──
void checkAutoLight(int lux) {
  if (isAutoLightMode) { // ถ้ากดแมนนวลบรรทัดนี้ false
    if (lux < 10) {
        setLight(true); // เปิดไฟเมื่อห้องมืด
    } else {
        setLight(false); // ปิดไฟเมื่อห้องสว่างปกติ
    }
  }
}
