/*
 * ESP32-C3 Sensor Controller
 * ===========================
 * Reads sensor data and sends it to a central hub via HTTP POST.
 * Blinks the onboard LED to show it's alive.
 *
 * Getting started:
 *   1. Edit include/config.h with your WiFi credentials and hub IP
 *   2. Build:   pio run
 *   3. Flash:   pio run --target upload
 *   4. Monitor: pio device monitor
 */

#include "config.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_wifi.h>

// ── Forward declarations ────────────────────────────────────
void connectWiFi();
void blinkLED(int times, int delayMs);
float readTemperature();
int readMoisture();
void sendSensorData(float temperature, int moisture);

// ── Global state ────────────────────────────────────────────
unsigned long lastSensorRead = 0;
bool ledState = false;

// ============================================================
// Setup — runs once on boot
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial monitor time to connect

  Serial.println();
  Serial.println("========================================");
  Serial.println("  ESP32 Sensor Controller");
  Serial.println("========================================");
  Serial.println();

  // Configure LED
  pinMode(LED_PIN, OUTPUT);
  blinkLED(3, 200); // Quick triple-blink to show we're alive

  // Connect to WiFi
  connectWiFi();

  Serial.println();
  Serial.println("Setup complete! Starting sensor loop...");
  Serial.println("────────────────────────────────────────");
}

// ============================================================
// Loop — runs continuously
// ============================================================
void loop() {
  // Reconnect WiFi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WARN] WiFi disconnected, reconnecting...");
    connectWiFi();
  }

  // Read and send sensor data at the configured interval
  unsigned long now = millis();
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL_MS) {
    lastSensorRead = now;

    // Read sensors
    float temperature = readTemperature();
    int moisture = readMoisture();

    // Print to serial
    Serial.printf("[DATA] Temp: %.1f°C  |  Moisture: %d%%  |  RSSI: %d dBm  |  "
                  "Uptime: %lu s\n",
                  temperature, moisture, WiFi.RSSI(), now / 1000);

    // Send to hub
    sendSensorData(temperature, moisture);

    // Toggle LED to show activity
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }
}

// ============================================================
// WiFi connection — with PMF disabled for WPA2/WPA3 routers
// ============================================================
void connectWiFi() {
  Serial.printf("[WiFi] Connecting to \"%s\"\n", WIFI_SSID);

  // Full reset
  WiFi.disconnect(true, true); // disconnect + erase stored credentials
  delay(200);
  WiFi.mode(WIFI_STA);

  // Register event handler for debug info
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WiFi] Associated with AP");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WiFi] Got IP: %s\n", WiFi.localIP().toString().c_str());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[WiFi] Disconnected, reason: %d\n",
                    info.wifi_sta_disconnected.reason);
      break;
    default:
      break;
    }
  });

  // Start WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  delay(100);

  // Disable PMF (Protected Management Frames) via ESP-IDF API
  // This fixes "reason 15" on routers using WPA2/WPA3 transition mode
  wifi_config_t wifi_cfg;
  esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg);
  wifi_cfg.sta.pmf_cfg.capable = false;
  wifi_cfg.sta.pmf_cfg.required = false;
  esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);

  Serial.println("[WiFi] PMF disabled, waiting for connection...");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Connected!");
    Serial.printf("[WiFi] IP address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi] Signal strength: %d dBm\n", WiFi.RSSI());
    Serial.printf("[WiFi] MAC: %s\n", WiFi.macAddress().c_str());
  } else {
    Serial.println("[WiFi] FAILED!");
    Serial.printf("[WiFi] Status code: %d\n", WiFi.status());
    Serial.println("[WiFi] Will retry in the loop.");
  }
}

// ============================================================
// Read the ESP32 internal temperature sensor
// ============================================================
float readTemperature() {
  // Reads the chip's internal temperature (not ambient)
  float temp = temperatureRead();
  return temp;
}

// ============================================================
// Read the soil moisture sensor (analog on D34)
// Returns 0-100 where 100 = fully wet, 0 = completely dry
// ============================================================
int readMoisture() {
  // Raw ADC: 0 (wet / sensor in water) to ~4095 (dry / sensor in air)
  int raw = analogRead(MOISTURE_PIN);
  // Invert and map to 0-100%
  int percent = map(raw, 4095, 0, 0, 100);
  percent = constrain(percent, 0, 100);
  return percent;
}

// ============================================================
// Send sensor data to the central hub via HTTP POST
// ============================================================
void sendSensorData(float temperature, int moisture) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] Skipping — WiFi not connected");
    return;
  }

  HTTPClient http;
  String url =
      String("http://") + HUB_HOST + ":" + String(HUB_PORT) + HUB_ENDPOINT;

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  // Build JSON payload
  String payload = "{";
  payload += "\"device_id\":\"esp32c3-01\",";
  payload += "\"temperature\":" + String(temperature, 1) + ",";
  payload += "\"moisture\":" + String(moisture) + ",";
  payload += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  payload += "\"uptime\":" + String(millis() / 1000);
  payload += "}";

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
// Blink the LED a given number of times
// ============================================================
void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(delayMs);
    digitalWrite(LED_PIN, LOW);
    delay(delayMs);
  }
}
