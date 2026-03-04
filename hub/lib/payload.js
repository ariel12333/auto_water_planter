/**
 * OOP Payload parser/builder for the ESP32 Sensor Hub.
 * Handles the nested format: { device_id: "...", sensors: [ { name: "Moisture", measurement: 45, units: "%" } ] }
 */

const BOARD_FIELDS = ['device_id', 'sensor_name', 'temperature', 'rssi', 'uptime', 'boot_count'];

/**
 * Parse and normalize incoming sensor data.
 * Flattens the nested `sensors` array into the root object for dashboard compatibility.
 * e.g. { sensors: [ { name: "Moisture", measurement: 45 } ] } -> { moisture: 45 }
 */
function parse(body) {
    const data = {};

    // Extract inherent board fields
    for (const f of BOARD_FIELDS) {
        if (body[f] !== undefined) {
            data[f] = body[f];
        }
    }

    // Fallback defaults
    if (!data.device_id) data.device_id = 'unknown';
    if (!data.sensor_name) data.sensor_name = data.device_id;

    // Flatten the dynamic sensors array
    if (Array.isArray(body.sensors)) {
        for (const s of body.sensors) {
            if (s.name && s.measurement !== undefined) {
                // Lowercase the name for the frontend (e.g. "Moisture" -> "moisture")
                const key = s.name.toLowerCase();
                data[key] = s.measurement;
                // Could also save units: data[`${key}_units`] = s.units;
            }
        }
    } else {
        // Legacy fallback (just in case)
        if (body.moisture !== undefined) data.moisture = body.moisture;
    }

    data.timestamp = new Date().toISOString();
    data.receivedAt = Date.now();
    return data;
}

/**
 * CSV header row.
 * We include the known board fields + moisture by default, but this could be dynamic.
 */
function csvHeader() {
    return [...BOARD_FIELDS, 'moisture', 'timestamp'].join(',');
}

/**
 * Convert a data object to a CSV row.
 */
function toCSVRow(data) {
    const allFields = [...BOARD_FIELDS, 'moisture', 'timestamp'];
    return allFields.map(f => data[f] ?? '').join(',');
}

/**
 * Get the display name for a device.
 */
function displayName(data) {
    return data.sensor_name || data.device_id || 'unknown';
}

module.exports = { parse, csvHeader, toCSVRow, displayName };
