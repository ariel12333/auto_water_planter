#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "config.h"
#include "constants.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

void connectWiFi() {
  Serial.printf("[WiFi] Connecting to \"%s\"\n", appConfig.wifi_ssid.c_str());

  WiFi.disconnect(true, true);
  delay(WIFI_DISCONNECT_DELAY_MS);
  WiFi.mode(WIFI_STA);

  WiFi.begin(appConfig.wifi_ssid.c_str(), appConfig.wifi_pass.c_str());
  delay(WIFI_CONNECT_DELAY_MS);

  // Disable PMF (fixes "reason 15" on WPA2/WPA3 transition routers)
  wifi_config_t wifi_cfg;
  if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) == ESP_OK) {
    wifi_cfg.sta.pmf_cfg.capable = false;
    wifi_cfg.sta.pmf_cfg.required = false;
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
  }

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_MAX_ATTEMPTS) {
    delay(WIFI_RETRY_DELAY_MS);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Connected!");
    Serial.printf("[WiFi] IP: %s  |  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("[WiFi] FAILED!");
    Serial.printf("[WiFi] Status: %d — will retry.\n", WiFi.status());
  }
}

#endif // WIFI_MANAGER_H
