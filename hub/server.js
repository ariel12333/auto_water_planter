const express = require('express');
const cors = require('cors');
const path = require('path');

const app = express();
const PORT = 8080;

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// ── In-memory sensor data store ─────────────────────────────
const MAX_HISTORY = 500;
const sensorData = {
  latest: null,
  history: [],
  devices: {},
  totalReceived: 0
};

// ── API: Receive sensor data from ESP32 ─────────────────────
app.post('/api/sensor-data', (req, res) => {
  const data = {
    ...req.body,
    timestamp: new Date().toISOString(),
    receivedAt: Date.now()
  };

  sensorData.latest = data;
  sensorData.history.push(data);
  sensorData.totalReceived++;

  // Track per-device
  const deviceId = data.device_id || 'unknown';
  sensorData.devices[deviceId] = {
    lastSeen: data.timestamp,
    lastData: data,
    count: (sensorData.devices[deviceId]?.count || 0) + 1
  };

  // Trim history
  if (sensorData.history.length > MAX_HISTORY) {
    sensorData.history = sensorData.history.slice(-MAX_HISTORY);
  }

  console.log(`[${new Date().toLocaleTimeString()}] 📡 ${deviceId}: temp=${data.temperature}°C moisture=${data.moisture ?? '--'}% rssi=${data.rssi}dBm`);
  res.json({ status: 'ok', received: sensorData.totalReceived });
});

// ── API: Get latest data ────────────────────────────────────
app.get('/api/sensor-data/latest', (req, res) => {
  res.json(sensorData.latest || { message: 'No data received yet' });
});

// ── API: Get history ────────────────────────────────────────
app.get('/api/sensor-data/history', (req, res) => {
  const limit = parseInt(req.query.limit) || 100;
  res.json(sensorData.history.slice(-limit));
});

// ── API: Get all device info ────────────────────────────────
app.get('/api/devices', (req, res) => {
  res.json(sensorData.devices);
});

// ── API: Get stats ──────────────────────────────────────────
app.get('/api/stats', (req, res) => {
  const temps = sensorData.history.map(d => d.temperature).filter(t => t != null);
  res.json({
    totalReceived: sensorData.totalReceived,
    deviceCount: Object.keys(sensorData.devices).length,
    historySize: sensorData.history.length,
    temperature: temps.length > 0 ? {
      min: Math.min(...temps),
      max: Math.max(...temps),
      avg: (temps.reduce((a, b) => a + b, 0) / temps.length).toFixed(1)
    } : null
  });
});

// Start
app.listen(PORT, '0.0.0.0', () => {
  console.log('');
  console.log('  ╔══════════════════════════════════════════╗');
  console.log('  ║       ESP32 Sensor Hub — Running ✅      ║');
  console.log('  ╠══════════════════════════════════════════╣');
  console.log(`  ║  Dashboard:  http://localhost:${PORT}        ║`);
  console.log(`  ║  API:        http://localhost:${PORT}/api    ║`);
  console.log(`  ║  Network:    http://10.0.0.36:${PORT}       ║`);
  console.log('  ╚══════════════════════════════════════════╝');
  console.log('');
  console.log('  Waiting for ESP32 data...');
  console.log('');
});
