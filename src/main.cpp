/*
 * ESP32 Sensor Controller
 * ===========================
 * Reads sensor data and sends it to a central hub via HTTP POST.
 * Supports deep sleep, light sleep, and active modes based on config.
 */

#include <Arduino.h>

#include "config.h"
#include "constants.h"
#include "payload.h"

// ── Shared Global Objects ─────────────────────────────────────
ConfigReader appConfig;
Board board;
RTC_DATA_ATTR int bootCount = 0;
unsigned long lastSensorRead = 0;
bool ledState = false;

// ── Manager Includes ──────────────────────────────────────────
#include "sensor_manager.h"
#include "sleep_manager.h"
#include "wifi_manager.h"

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

// ============================================================
// Setup — runs once on boot (and on every deep sleep wake)
// ============================================================
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(STARTUP_DELAY_MS);

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
  pinMode(LED_PIN, OUTPUT);
  blinkLED(2, BLINK_DELAY_MS);

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
        (unsigned long)appConfig.settings.read_interval_s * MILLIS_TO_SECONDS) {
      lastSensorRead = now;
      readAndSend();
    }
  }
}
