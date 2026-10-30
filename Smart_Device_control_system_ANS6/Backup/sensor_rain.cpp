#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "mqtt_manager.h"
#include "sensor_rain.h"

// ============================================================
//  Rain Sensor
// ============================================================
bool readRainAndPublish() {
  // ── Rain Sensor ──
  int   rawRain  = analogRead(PIN_RAIN_AO);
  float rainPct  = constrain(
    map(rawRain, RAIN_DRY, RAIN_WET, 0, 100), 0, 100
  );
  bool  isRain   = (digitalRead(PIN_RAIN_DO) == LOW) || (rainPct >= RAIN_THRESHOLD_PCT);

  publishFloat(TOPIC_RAIN_PCT, rainPct, 1);
  publishStr(TOPIC_RAIN_DIG, isRain ? "RAIN" : "DRY");
  Serial.printf("│ [Rain]   Intensity=%.1f%%  State=%s  (raw=%d)\n",
                rainPct, isRain ? "RAIN" : "DRY", rawRain);

  return isRain;
}
