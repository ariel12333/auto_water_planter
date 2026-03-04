/*
 * ESP32 Sensor Controller
 * ===========================
 * Reads sensor data and sends it to a central hub via HTTP POST.
 * Supports deep sleep, light sleep, and active modes based on config.
 *
 * Sleep behaviour (configured in config.h):
 *   - interval >= DEEP_SLEEP_THRESHOLD_S  → deep sleep between readings
 *   - interval >= LIGHT_SLEEP_THRESHOLD_S → light sleep between readings
 *   - interval < LIGHT_SLEEP_THRESHOLD_S  → stays awake, polls in loop()
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
#include <esp_sleep.h>
#include <esp_wifi.h>

// ── Forward declarations ────────────────────────────────────
void connectWiFi();
void blinkLED(int times, int delayMs);
float readTemperature();
int readMoisture();
void sendSensorData(float temperature, int moisture);
void readAndSend();
void enterDeepSleep();
void enterLightSleep();

// ── Compile-time sleep mode selection ───────────────────────
#define SLEEP_MODE_NONE 0
#define SLEEP_MODE_LIGHT 1
#define SLEEP_MODE_DEEP 2

#if SENSOR_READ_INTERVAL_S >= DEEP_SLEEP_THRESHOLD_S
#define CURRENT_SLEEP_MODE SLEEP_MODE_DEEP
#elif SENSOR_READ_INTERVAL_S >= LIGHT_SLEEP_THRESHOLD_S
#define CURRENT_SLEEP_MODE SLEEP_MODE_LIGHT
#else
#define CURRENT_SLEEP_MODE SLEEP_MODE_NONE
#endif

// ── RTC memory — survives deep sleep ────────────────────────
RTC_DATA_ATTR int bootCount = 0;

// ── Global state ────────────────────────────────────────────
unsigned long lastSensorRead = 0;
bool ledState = false;

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
  Serial.printf("  Boot #%d | Interval: %ds", bootCount,
                SENSOR_READ_INTERVAL_S);

#if CURRENT_SLEEP_MODE == SLEEP_MODE_DEEP
  Serial.println(" | Mode: DEEP SLEEP");
#elif CURRENT_SLEEP_MODE == SLEEP_MODE_LIGHT
  Serial.println(" | Mode: LIGHT SLEEP");
#else
  Serial.println(" | Mode: ACTIVE");
#endif

  Serial.println("========================================");
  Serial.println();

  // Configure LED
  pinMode(LED_PIN, OUTPUT);
  blinkLED(2, 150);

  // Connect to WiFi
  connectWiFi();

  Serial.println();
  Serial.println("Setup complete!");
  Serial.println("────────────────────────────────────────");

  // In deep sleep mode: read/send immediately, then sleep
  // (loop() never runs meaningfully in deep sleep mode)
#if CURRENT_SLEEP_MODE == SLEEP_MODE_DEEP
  readAndSend();
  enterDeepSleep();
#endif
}

// ============================================================
// Loop — only used for ACTIVE and LIGHT SLEEP modes
// ============================================================
void loop() {
#if CURRENT_SLEEP_MODE == SLEEP_MODE_DEEP
  // Should never reach here — deep sleep resets the chip
  enterDeepSleep();
  return;
#endif

  // Reconnect WiFi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WARN] WiFi disconnected, reconnecting...");
    connectWiFi();
  }

#if CURRENT_SLEEP_MODE == SLEEP_MODE_LIGHT
  // Light sleep mode: read, send, then sleep
  readAndSend();
  enterLightSleep();
#else
  // Active mode: poll at the configured interval using millis()
  unsigned long now = millis();
  if (now - lastSensorRead >= (unsigned long)SENSOR_READ_INTERVAL_S * 1000UL) {
    lastSensorRead = now;
    readAndSend();
  }
#endif
}

// ============================================================
// Read sensors and send data
// ============================================================
void readAndSend() {
  float temperature = readTemperature();
  int moisture = readMoisture();

  Serial.printf("[DATA] Temp: %.1f°C  |  Moisture: %d%%  |  RSSI: %d dBm  |  "
                "Uptime: %lu s  |  Boot: %d\n",
                temperature, moisture, WiFi.RSSI(), millis() / 1000, bootCount);

  sendSensorData(temperature, moisture);

  // Blink LED to show activity
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);
}

// ============================================================
// Deep sleep — powers off CPUs, RAM, WiFi (~10µA)
// Full reboot on wake — setup() runs again
// ============================================================
void enterDeepSleep() {
  unsigned long sleepUs = (unsigned long)SENSOR_READ_INTERVAL_S * 1000000ULL;
  Serial.printf("[SLEEP] Deep sleep for %d seconds...\n",
                SENSOR_READ_INTERVAL_S);
  Serial.flush();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  esp_sleep_enable_timer_wakeup(sleepUs);
  esp_deep_sleep_start();
  // Does not return — chip resets on wake
}

// ============================================================
// Light sleep — pauses CPU, WiFi may stay associated (~2mA)
// Resumes execution after the sleep call
// ============================================================
void enterLightSleep() {
  unsigned long sleepUs = (unsigned long)SENSOR_READ_INTERVAL_S * 1000000ULL;
  Serial.printf("[SLEEP] Light sleep for %d seconds...\n",
                SENSOR_READ_INTERVAL_S);
  Serial.flush();

  esp_sleep_enable_timer_wakeup(sleepUs);
  esp_light_sleep_start();

  // Execution resumes here after wake
  Serial.println("[SLEEP] Woke up from light sleep");
}

// ============================================================
// WiFi connection — with PMF disabled for WPA2/WPA3 routers
// ============================================================
void connectWiFi() {
  Serial.printf("[WiFi] Connecting to \"%s\"\n", WIFI_SSID);

  WiFi.disconnect(true, true);
  delay(200);
  WiFi.mode(WIFI_STA);

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

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
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
// Read the ESP32 internal temperature sensor
// ============================================================
float readTemperature() { return temperatureRead(); }

// ============================================================
// Read the soil moisture sensor (analog on D34)
// Returns 0-100 where 100 = fully wet, 0 = completely dry
// ============================================================
int readMoisture() {
  int raw = analogRead(MOISTURE_PIN);
  int percent = map(raw, 4095, 0, 0, 100);
  return constrain(percent, 0, 100);
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

  String payload = "{";
  payload += "\"device_id\":\"esp32c3-01\",";
  payload += "\"temperature\":" + String(temperature, 1) + ",";
  payload += "\"moisture\":" + String(moisture) + ",";
  payload += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  payload += "\"uptime\":" + String(millis() / 1000) + ",";
  payload += "\"boot_count\":" + String(bootCount);
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
