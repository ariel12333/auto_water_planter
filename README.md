# 🌱 Auto Water Planter

ESP32-based automated plant watering system with a real-time web dashboard.

Reads soil moisture via an analog sensor, displays live data on a self-hosted dashboard, and (soon) controls a water pump automatically.

## Architecture

```
┌──────────────┐      WiFi / HTTP POST      ┌──────────────────┐
│   ESP32      │  ─────────────────────────▶ │   Hub Server     │
│  + Moisture  │   every 2s: temp,          │   (Node.js)      │
│    Sensor    │   moisture, rssi, uptime   │   + Dashboard    │
└──────────────┘                             └──────────────────┘
```

## Quick Start

### 1. Clone & configure

```bash
git clone https://github.com/ariel12333/auto_water_planter.git
cd auto_water_planter

# Create your config from the template
cp include/config.h.example include/config.h
# Edit with your WiFi credentials and hub IP
nano include/config.h
```

### 2. Flash the ESP32

Requires [PlatformIO CLI](https://platformio.org/install/cli).

```bash
pio run --target upload    # Build & flash
pio device monitor         # View serial output
```

### 3. Run the hub server

**Option A — Docker (recommended):**
```bash
cd hub
docker build -t esp32-hub .
docker run -d -p 8080:8080 --name esp32-hub esp32-hub
```

**Option B — Node.js directly:**
```bash
cd hub
npm install
npm start
```

Open `http://localhost:8080` to view the dashboard.

## Wiring

| Sensor / Module | Pin | ESP32 Pin |
|-----------------|-----|-----------|
| MH-Sensor **VCC** | → | **3V3** |
| MH-Sensor **GND** | → | **GND** |
| MH-Sensor **AO** | → | **D34** |
| Relay **VCC** | → | **VIN** |
| Relay **GND** | → | **GND** |
| Relay **IN** | → | **D26** |

The pump connects through the relay using a **separate 5V power supply** (not the ESP32).

## Project Structure

```
├── src/main.cpp              # ESP32 firmware
├── include/config.h.example  # Config template (WiFi, hub IP)
├── platformio.ini            # PlatformIO build config
└── hub/
    ├── server.js             # Express API server
    ├── public/index.html     # Real-time dashboard
    └── Dockerfile            # Lightweight Alpine container
```

## License

MIT
