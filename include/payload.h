#ifndef PAYLOAD_H
#define PAYLOAD_H

#include <Arduino.h>

/*
 * Payload builder for ESP32 sensor data.
 * Field names match shared/payload_schema.json.
 *
 * Usage:
 *   String json = Payload::build("esp32-01", "Garden", 23.5, 62, -68, 120, 3);
 *
 * To add a new sensor field:
 *   1. Add it to shared/payload_schema.json
 *   2. Add a parameter + line here
 *   3. Add parsing in hub/lib/payload.js
 */

namespace Payload {

inline String build(const char *deviceId, const char *sensorName,
                    float temperature, int moisture, int rssi,
                    unsigned long uptime, int bootCount) {
  String json = "{";
  json += "\"device_id\":\"" + String(deviceId) + "\",";
  json += "\"sensor_name\":\"" + String(sensorName) + "\",";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"moisture\":" + String(moisture) + ",";
  json += "\"rssi\":" + String(rssi) + ",";
  json += "\"uptime\":" + String(uptime) + ",";
  json += "\"boot_count\":" + String(bootCount);
  json += "}";
  return json;
}

} // namespace Payload

#endif // PAYLOAD_H
