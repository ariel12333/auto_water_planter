#ifndef PAYLOAD_H
#define PAYLOAD_H

#include <Arduino.h>
#include <vector>

// 1) Sensor Struct
struct Sensor {
  String name;
  float measurement;
  String units;
  int channel_in;
  int channel_out;

  // Helper to format as a JSON object string
  String toJSON() const {
    String json = "{";
    json += "\"name\":\"" + name + "\",";
    json += "\"measurement\":" + String(measurement, 2) + ",";
    json += "\"units\":\"" + units + "\",";
    json += "\"channel_in\":" + String(channel_in) + ",";
    json += "\"channel_out\":" + String(channel_out);
    json += "}";
    return json;
  }
};

// 2) Payload Class
class Payload {
public:
  std::vector<Sensor> sensors;

  void addSensor(const Sensor &s) { sensors.push_back(s); }

  // Generates the JSON array of sensors
  String create_payload() const {
    String json = "[";
    for (size_t i = 0; i < sensors.size(); i++) {
      json += sensors[i].toJSON();
      if (i < sensors.size() - 1) {
        json += ",";
      }
    }
    json += "]";
    return json;
  }

  // Stub for future use
  void parse_payload(const String &jsonStr) {
    // Not implemented
  }
};

// 3) Board Class
class Board {
public:
  String device_id;
  String name;
  Payload payload;

  Board(String id, String n) : device_id(id), name(n) {}

  // Initialize state of sensors (add them to the payload)
  void init_sensors(int moisture_pin, float moisture_val) {
    payload.sensors.clear();

    // Add the moisture sensor
    Sensor mSensor;
    mSensor.name = "Moisture";
    mSensor.measurement = moisture_val;
    mSensor.units = "%";
    mSensor.channel_in = moisture_pin;
    mSensor.channel_out = -1; // Unused

    payload.addSensor(mSensor);
  }

  // Create the final JSON payload containing inherent sensors and the custom
  // payload array
  String create_payload(float temperature, int rssi, unsigned long uptime,
                        int bootCount) {
    String json = "{";
    json += "\"device_id\":\"" + device_id + "\",";
    json += "\"sensor_name\":\"" + name + "\",";
    json += "\"temperature\":" + String(temperature, 1) + ",";
    json += "\"rssi\":" + String(rssi) + ",";
    json += "\"uptime\":" + String(uptime) + ",";
    json += "\"boot_count\":" + String(bootCount) + ",";
    json += "\"sensors\":" + payload.create_payload();
    json += "}";
    return json;
  }
};

#endif // PAYLOAD_H
