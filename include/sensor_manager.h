#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "config.h"
#include "constants.h"
#include "payload.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

extern Board board;
extern int bootCount;
extern bool ledState;

// ============================================================
// Internal temperature sensor
// ============================================================
float readTemperature() { return temperatureRead(); }

// ============================================================
// Dynamically read all sensors configured on the board
// ============================================================
void readSensors() {
  for (size_t i = 0; i < board.payload.sensors.size(); i++) {
    int pin = board.payload.sensors[i].channel_in;
    if (pin >= 0) {
      int raw = analogRead(pin);
      // Default moisture sensor assumption: map ADC to 0-100%
      int percent = map(raw, MOISTURE_ADC_MAX, MOISTURE_ADC_MIN,
                        MOISTURE_PERCENT_MIN, MOISTURE_PERCENT_MAX);
      percent = constrain(percent, MOISTURE_PERCENT_MIN, MOISTURE_PERCENT_MAX);

      board.payload.sensors[i].measurement = percent;
      Serial.printf("[Sensor] %s : %d%s\n",
                    board.payload.sensors[i].name.c_str(), percent,
                    board.payload.sensors[i].units.c_str());
    }
  }
}

// ============================================================
// Send payload to the JSON Hub
// ============================================================
void sendSensorData(float temperature) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] Skipping — WiFi not connected");
    return;
  }

  HTTPClient http;
  String url = "http://" + appConfig.hub_host + ":" +
               String(appConfig.hub_port) + appConfig.hub_endpoint;

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  // Build payload using our OOP Board class
  String payload = board.create_payload(
      temperature, WiFi.RSSI(), millis() / MILLIS_TO_SECONDS, bootCount);

  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    Serial.printf("[HTTP] POST → %d\n", httpCode);
  } else {
    Serial.printf("[HTTP] POST failed: %s\n",
                  http.errorToString(httpCode).c_str());
  }

  http.end();
}

// ============================================================
// Read sensors and send data
// ============================================================
void readAndSend() {
  float temperature = readTemperature();
  readSensors(); // Updates the measurements on the board payload

  Serial.printf(
      "[DATA] Temp: %.1f°C  | RSSI: %d dBm  | Uptime: %lu s | Boot: %d\n",
      temperature, WiFi.RSSI(), millis() / MILLIS_TO_SECONDS, bootCount);

  sendSensorData(temperature);

  // Blink LED to show activity
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);
}

#endif // SENSOR_MANAGER_H
