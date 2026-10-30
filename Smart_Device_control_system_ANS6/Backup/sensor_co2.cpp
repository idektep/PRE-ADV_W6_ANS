#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "mqtt_manager.h"
#include "sensor_co2.h"

// ============================================================
//  MQ135: CO2
// ============================================================
void readCO2AndPublish() {
  // ── MQ135: CO2 ──
  int   rawMQ  = analogRead(PIN_MQ135);
  //int   rawMQ  = digitalRead(PIN_MQ135);
  float vMQ    = rawMQ * (3.3f / 4095.0f);
  float rsMQ   = (vMQ > 0.01f) ? (MQ135_RL * (3.3f - vMQ) / vMQ) : MQ135_RL * 100;
  float ratio  = rsMQ / MQ135_RO;
  float co2    = constrain(MQ135_A * powf(ratio, MQ135_B), 300.0f, 5000.0f);

  publishFloat(TOPIC_CO2, co2, 0);
  Serial.printf("│ [MQ135]  CO2=%.0f ppm  (raw=%d)\n", co2, rawMQ);
}
