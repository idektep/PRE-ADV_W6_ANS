#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "mqtt_manager.h"
#include "sensor_soil.h"

// ============================================================
//  Soil Moisture
// ============================================================
float readSoilAndPublish() {
  // ── Soil Moisture ──
  int   rawSoil = analogRead(PIN_SOIL);
  float soilPct = constrain(
    map(rawSoil, SOIL_DRY, SOIL_WET, 0, 100), 0, 100
  );
  publishFloat(TOPIC_SOIL, soilPct, 1);
  Serial.printf("│ [Soil]   Moisture=%.1f%%  (raw=%d)\n", soilPct, rawSoil);

  return soilPct;
}
