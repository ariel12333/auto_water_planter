/**
 * Payload parser/builder for the ESP32 Sensor Hub.
 * Field names match shared/payload_schema.json.
 *
 * To add a new sensor field:
 *   1. Add it to shared/payload_schema.json
 *   2. Add it to FIELDS below
 *   3. Add a line in include/payload.h on the ESP32 side
 */

const FIELDS = [
    { name: 'device_id', type: 'string', required: true },
    { name: 'sensor_name', type: 'string', required: false },
    { name: 'temperature', type: 'number', required: false },
    { name: 'moisture', type: 'number', required: false },
    { name: 'rssi', type: 'number', required: false },
    { name: 'uptime', type: 'number', required: false },
    { name: 'boot_count', type: 'number', required: false },
];

/**
 * Parse and normalize incoming sensor data.
 * Adds timestamp + receivedAt. Returns the enriched object.
 */
function parse(body) {
    const data = {};

    for (const f of FIELDS) {
        if (body[f.name] !== undefined) {
            data[f.name] = body[f.name];
        } else if (f.required) {
            data[f.name] = f.type === 'string' ? 'unknown' : 0;
        }
    }

    data.timestamp = new Date().toISOString();
    data.receivedAt = Date.now();
    return data;
}

/**
 * CSV header row.
 */
function csvHeader() {
    return FIELDS.map(f => f.name).join(',');
}

/**
 * Convert a data object to a CSV row.
 */
function toCSVRow(data) {
    return FIELDS.map(f => data[f.name] ?? '').join(',');
}

/**
 * Get the display name for a device (sensor_name or device_id fallback).
 */
function displayName(data) {
    return data.sensor_name || data.device_id || 'unknown';
}

module.exports = { FIELDS, parse, csvHeader, toCSVRow, displayName };
