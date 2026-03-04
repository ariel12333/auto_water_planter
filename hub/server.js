const express = require('express');
const cors = require('cors');
const path = require('path');

const app = express();
const PORT = 8080;

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// ── Configuration ───────────────────────────────────────────
const MAX_HISTORY_PER_DEVICE = 1000;

// ── In-memory sensor data store (per-device) ────────────────
const deviceStore = {};
// deviceStore[deviceId] = { latest, history[], count }

let totalReceived = 0;

function getOrCreateDevice(deviceId) {
  if (!deviceStore[deviceId]) {
    deviceStore[deviceId] = {
      latest: null,
      history: [],
      count: 0,
      firstSeen: new Date().toISOString()
    };
  }
  return deviceStore[deviceId];
}

// ── API: Receive sensor data from ESP32 ─────────────────────
app.post('/api/sensor-data', (req, res) => {
  const data = {
    ...req.body,
    timestamp: new Date().toISOString(),
    receivedAt: Date.now()
  };

  const deviceId = data.device_id || 'unknown';
  const device = getOrCreateDevice(deviceId);

  device.latest = data;
  device.history.push(data);
  device.count++;
  totalReceived++;

  // Trim history per device
  if (device.history.length > MAX_HISTORY_PER_DEVICE) {
    device.history = device.history.slice(-MAX_HISTORY_PER_DEVICE);
  }

  console.log(`[${new Date().toLocaleTimeString()}] 📡 ${deviceId}: temp=${data.temperature}°C moisture=${data.moisture ?? '--'}% rssi=${data.rssi}dBm`);
  res.json({ status: 'ok', received: totalReceived });
});

// ── API: Get latest data (global or per-device) ─────────────
app.get('/api/sensor-data/latest', (req, res) => {
  const deviceId = req.query.device;
  if (deviceId && deviceStore[deviceId]) {
    return res.json(deviceStore[deviceId].latest);
  }
  // Return latest across all devices
  let latest = null;
  for (const d of Object.values(deviceStore)) {
    if (!latest || (d.latest && d.latest.receivedAt > latest.receivedAt)) {
      latest = d.latest;
    }
  }
  res.json(latest || { message: 'No data received yet' });
});

// ── API: Get history (per-device or all) ────────────────────
app.get('/api/sensor-data/history', (req, res) => {
  const deviceId = req.query.device;
  const limit = parseInt(req.query.limit) || MAX_HISTORY_PER_DEVICE;

  if (deviceId && deviceStore[deviceId]) {
    return res.json(deviceStore[deviceId].history.slice(-limit));
  }

  // Merge all device histories, sorted by receivedAt
  const all = Object.values(deviceStore)
    .flatMap(d => d.history)
    .sort((a, b) => a.receivedAt - b.receivedAt);
  res.json(all.slice(-limit));
});

// ── API: Get all devices ────────────────────────────────────
app.get('/api/devices', (req, res) => {
  const devices = {};
  for (const [id, d] of Object.entries(deviceStore)) {
    devices[id] = {
      lastSeen: d.latest?.timestamp,
      count: d.count,
      firstSeen: d.firstSeen,
      historySize: d.history.length
    };
  }
  res.json(devices);
});

// ── API: Get stats (global or per-device) ───────────────────
app.get('/api/stats', (req, res) => {
  const deviceId = req.query.device;
  const history = deviceId && deviceStore[deviceId]
    ? deviceStore[deviceId].history
    : Object.values(deviceStore).flatMap(d => d.history);

  const temps = history.map(d => d.temperature).filter(t => t != null);
  const moistures = history.map(d => d.moisture).filter(m => m != null);

  res.json({
    totalReceived,
    deviceCount: Object.keys(deviceStore).length,
    historySize: history.length,
    maxHistory: MAX_HISTORY_PER_DEVICE,
    temperature: temps.length > 0 ? {
      min: Math.min(...temps),
      max: Math.max(...temps),
      avg: (temps.reduce((a, b) => a + b, 0) / temps.length).toFixed(1)
    } : null,
    moisture: moistures.length > 0 ? {
      min: Math.min(...moistures),
      max: Math.max(...moistures),
      avg: (moistures.reduce((a, b) => a + b, 0) / moistures.length).toFixed(1)
    } : null
  });
});

// ── API: CSV export ─────────────────────────────────────────
app.get('/api/export/csv', (req, res) => {
  const deviceId = req.query.device;
  const history = deviceId && deviceStore[deviceId]
    ? deviceStore[deviceId].history
    : Object.values(deviceStore).flatMap(d => d.history).sort((a, b) => a.receivedAt - b.receivedAt);

  if (history.length === 0) {
    return res.status(404).send('No data to export');
  }

  const header = 'timestamp,device_id,temperature,moisture,rssi,uptime,boot_count';
  const rows = history.map(d =>
    `${d.timestamp},${d.device_id || ''},${d.temperature ?? ''},${d.moisture ?? ''},${d.rssi ?? ''},${d.uptime ?? ''},${d.boot_count ?? ''}`
  );

  const csv = [header, ...rows].join('\n');
  const filename = deviceId ? `sensor_data_${deviceId}.csv` : 'sensor_data_all.csv';

  res.setHeader('Content-Type', 'text/csv');
  res.setHeader('Content-Disposition', `attachment; filename="${filename}"`);
  res.send(csv);
});

// Start
app.listen(PORT, '0.0.0.0', () => {
  console.log('');
  console.log('  ╔══════════════════════════════════════════╗');
  console.log('  ║       ESP32 Sensor Hub — Running ✅      ║');
  console.log('  ╠══════════════════════════════════════════╣');
  console.log(`  ║  Dashboard:  http://localhost:${PORT}        ║`);
  console.log(`  ║  API:        http://localhost:${PORT}/api    ║`);
  console.log(`  ║  Max history: ${MAX_HISTORY_PER_DEVICE} per device         ║`);
  console.log('  ╚══════════════════════════════════════════╝');
  console.log('');
  console.log('  Waiting for ESP32 data...');
  console.log('');
});
