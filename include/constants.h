#ifndef CONSTANTS_H
#define CONSTANTS_H

// ── Pin Definitions ──────────────────────────────────────────
#ifndef LED_PIN
#define LED_PIN 2
#endif

// ── Serial Settings ─────────────────────────────────────────
#define SERIAL_BAUD_RATE 115200

// ── Timing & Delays ─────────────────────────────────────────
#define STARTUP_DELAY_MS 500
#define BLINK_DELAY_MS 150
#define WIFI_RETRY_DELAY_MS 500
#define WIFI_CONNECT_DELAY_MS 100
#define WIFI_DISCONNECT_DELAY_MS 200

// ── WiFi Settings ───────────────────────────────────────────
#define WIFI_MAX_ATTEMPTS 60

// ── Sensor Settings ─────────────────────────────────────────
#define MOISTURE_ADC_MAX 4095
#define MOISTURE_ADC_MIN 0
#define MOISTURE_PERCENT_MAX 100
#define MOISTURE_PERCENT_MIN 0
#define MILLIS_TO_SECONDS 1000UL
#define SECONDS_TO_MICROSECONDS 1000000ULL

// ── Sleep Modes ─────────────────────────────────────────────
#define SLEEP_MODE_NONE 0
#define SLEEP_MODE_LIGHT 1
#define SLEEP_MODE_DEEP 2

#endif // CONSTANTS_H
