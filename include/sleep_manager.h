#ifndef SLEEP_MANAGER_H
#define SLEEP_MANAGER_H

#include "config.h"
#include "constants.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>

int getSleepMode() {
  if (appConfig.settings.read_interval_s >=
      appConfig.settings.deep_sleep_threshold_s) {
    return SLEEP_MODE_DEEP;
  } else if (appConfig.settings.read_interval_s >=
             appConfig.settings.light_sleep_threshold_s) {
    return SLEEP_MODE_LIGHT;
  }
  return SLEEP_MODE_NONE;
}

void enterDeepSleep() {
  unsigned long sleepUs = (unsigned long)appConfig.settings.read_interval_s *
                          SECONDS_TO_MICROSECONDS;
  Serial.printf("[SLEEP] Deep sleep for %d seconds...\n",
                appConfig.settings.read_interval_s);
  Serial.flush();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  esp_sleep_enable_timer_wakeup(sleepUs);
  esp_deep_sleep_start();
}

void enterLightSleep() {
  unsigned long sleepUs = (unsigned long)appConfig.settings.read_interval_s *
                          SECONDS_TO_MICROSECONDS;
  Serial.printf("[SLEEP] Light sleep for %d seconds...\n",
                appConfig.settings.read_interval_s);
  Serial.flush();

  esp_sleep_enable_timer_wakeup(sleepUs);
  esp_light_sleep_start();

  Serial.println("[SLEEP] Woke up from light sleep");
}

#endif // SLEEP_MANAGER_H
