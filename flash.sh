#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

echo "========================================"
echo "    ESP32 Flashing Automation Script    "
echo "========================================"

# Make sure we are in the right directory
cd "$(dirname "$0")"

echo "[1/3] Building firmware..."
pio run

echo "[2/3] Uploading LittleFS image (config.json)..."
pio run --target uploadfs

echo "[3/3] Uploading Firmware..."
pio run --target upload

echo "========================================"
echo "   Successfully flashed the ESP32!      "
echo "========================================"

echo ""
echo "Do you want to open the serial monitor to view logs? (y/n)"
read -r -p "> " response
if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
    pio device monitor
fi
