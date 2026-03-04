/*
 * ESP32 Sensor Controller
 * ===========================
 * Reads sensor data and sends it to a central hub via HTTP POST.
 * Supports deep sleep, light sleep, and active modes based on config.
 */

#include "config.h"
#include "payload.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_wifi.h>

// ── Forward declarations ────────────────────────────────────
void connectWiFi();
void blinkLED(int times, int delayMs);
float readTemperature();
void readSensors();
void sendSensorData(float temperature);
void readAndSend();
void enterDeepSleep();
void enterLightSleep();
int getSleepMode();

// ── Sleep mode constants ────────────────────────────────────
#define SLEEP_MODE_NONE 0
#define SLEEP_MODE_LIGHT 1
#define SLEEP_MODE_DEEP 2

// ── RTC memory — survives deep sleep ────────────────────────
RTC_DATA_ATTR int bootCount = 0;

// ── Global state ────────────────────────────────────────────
unsigned long lastSensorRead = 0;
bool ledState = false;

// Global objects
ConfigReader appConfig;
Board board;

// ============================================================
// Runtime Sleep Mode Logic
// ============================================================
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

// ============================================================
// Setup — runs once on boot (and on every deep sleep wake)
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  bootCount++;

  Serial.println();
  Serial.println("========================================");
  Serial.println("  ESP32 Sensor Controller");
  Serial.println("========================================");

  // Load JSON Config from LittleFS
  if (appConfig.load()) {
    Serial.println("[Config] Loaded successfully from LittleFS");
  } else {
    Serial.println("[Config] Failed to load, using defaults");
  }

  Serial.printf("  Device ID:  %s\n", appConfig.device_id.c_str());
  Serial.printf("  Boot #%d | Interval: %ds\n", bootCount,
                appConfig.settings.read_interval_s);
  Serial.println("========================================");

  // Initialize Board
  board = Board(appConfig.device_id, appConfig.sensor_name);

  // Initialize dynamic sensors from JSON array
  JsonArray sensorsArray = appConfig.doc["sensors"].as<JsonArray>();
  for (JsonVariant v : sensorsArray) {
    Sensor s;
    s.name = v["name"] | "Unknown";
    s.units = v["units"] | "";
    s.channel_in = v["channel_in"] | -1;
    s.channel_out = v["channel_out"] | -1;
    s.measurement = 0;
    board.payload.addSensor(s);
    Serial.printf("[Sensor] Added: %s (pin %d)\n", s.name.c_str(),
                  s.channel_in);
  }

// Configure LED
// Ensure LED_PIN is defined; fallback to 2
#ifndef LED_PIN
#define LED_PIN 2
#endif
  pinMode(LED_PIN, OUTPUT);
  blinkLED(2, 150);

  // Connect to WiFi
  connectWiFi();

  Serial.println();
  Serial.println("Setup complete!");
  Serial.println("────────────────────────────────────────");

  // In deep sleep mode: read/send immediately, then sleep
  if (getSleepMode() == SLEEP_MODE_DEEP) {
    readAndSend();
    enterDeepSleep();
  }
}

// ============================================================
// Loop — only used for ACTIVE and LIGHT SLEEP modes
// ============================================================
void loop() {
  // Should never reach here if deep sleep is active — chip resets
  if (getSleepMode() == SLEEP_MODE_DEEP) {
    enterDeepSleep();
    return;
  }

  // Reconnect WiFi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WARN] WiFi disconnected, reconnecting...");
    connectWiFi();
  }

  if (getSleepMode() == SLEEP_MODE_LIGHT) {
    // Light sleep mode: read, send, then sleep
    readAndSend();
    enterLightSleep();
  } else {
    // Active mode: poll at the configured interval using millis()
    unsigned long now = millis();
    if (now - lastSensorRead >=
        (unsigned long)appConfig.settings.read_interval_s * 1000UL) {
      lastSensorRead = now;
      readAndSend();
    }
  }
}

// ============================================================
// Read sensors and send data
// ============================================================
void readAndSend() {
  float temperature = readTemperature();
  readSensors(); // Updates the measurements on the board payload

  Serial.printf(
      "[DATA] Temp: %.1f°C  | RSSI: %d dBm  | Uptime: %lu s | Boot: %d\n",
      temperature, WiFi.RSSI(), millis() / 1000, bootCount);

  sendSensorData(temperature);

  // Blink LED to show activity
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);
}

// ============================================================
// Dynamically read all sensors configured on the board
// ============================================================
void readSensors() {
  for (size_t i = 0; i < board.payload.sensors.size(); i++) {
    int pin = board.payload.sensors[i].channel_in;
    if (pin >= 0) {
      int raw = analogRead(pin);
      // For simple moisture, map 4095-0 to 0-100%
      int percent = map(raw, 4095, 0, 0, 100);
      percent = constrain(percent, 0, 100);
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
  String payload = board.create_payload(temperature, WiFi.RSSI(),
                                        millis() / 1000, bootCount);

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
// Deep sleep
// ============================================================
void enterDeepSleep() {
  unsigned long sleepUs =
      (unsigned long)appConfig.settings.read_interval_s * 1000000ULL;
  Serial.printf("[SLEEP] Deep sleep for %d seconds...\n",
                appConfig.settings.read_interval_s);
  Serial.flush();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  esp_sleep_enable_timer_wakeup(sleepUs);
  esp_deep_sleep_start();
}

// ============================================================
// Light sleep
// ============================================================
void enterLightSleep() {
  unsigned long sleepUs =
      (unsigned long)appConfig.settings.read_interval_s * 1000000ULL;
  Serial.printf("[SLEEP] Light sleep for %d seconds...\n",
                appConfig.settings.read_interval_s);
  Serial.flush();

  esp_sleep_enable_timer_wakeup(sleepUs);
  esp_light_sleep_start();

  Serial.println("[SLEEP] Woke up from light sleep");
}

// ============================================================
// WiFi connection
// ============================================================
void connectWiFi() {
  Serial.printf("[WiFi] Connecting to \"%s\"\n", appConfig.wifi_ssid.c_str());

  WiFi.disconnect(true, true);
  delay(200);
  WiFi.mode(WIFI_STA);

  WiFi.begin(appConfig.wifi_ssid.c_str(), appConfig.wifi_pass.c_str());
  delay(100);

  // Disable PMF (fixes "reason 15" on WPA2/WPA3 transition routers)
  wifi_config_t wifi_cfg;
  esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg);
  wifi_cfg.sta.pmf_cfg.capable = false;
  wifi_cfg.sta.pmf_cfg.required = false;
  esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    delay(500);
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

// ============================================================
// Internal temperature sensor
// ============================================================
float readTemperature() { return temperatureRead(); }

// ============================================================
// Blink the LED
// ============================================================
void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(delayMs);
    digitalWrite(LED_PIN, LOW);
    delay(delayMs);
  }
}
