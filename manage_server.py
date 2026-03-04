#!/usr/bin/env python3
import argparse
import subprocess
import sys

def run_cmd(cmd):
    """Run a shell command and return the exit code."""
    print(f"Executing: {cmd}")
    result = subprocess.run(cmd, shell=True)
    return result.returncode

def start_server():
    print("Starting ESP32 Sensor Hub Docker container...")
    # Will start the container if it exists, or run it if it doesn't
    res = run_cmd("sudo docker start esp32-hub")
    if res != 0:
        print("Container 'esp32-hub' might not exist. Attempting to build and run it...")
        run_cmd("sudo docker build -t esp32-hub ./hub")
        run_cmd("sudo docker run -d -p 8080:8080 --name esp32-hub esp32-hub")
    
def stop_server():
    print("Stopping ESP32 Sensor Hub Docker container...")
    run_cmd("sudo docker stop esp32-hub")

def clear_data():
    print("Clearing server data...")
    print("Because the current server uses in-memory storage, stopping and removing the container clears all data.")
    run_cmd("sudo docker stop esp32-hub")
    run_cmd("sudo docker rm esp32-hub")
    print("Data cleared. Run 'start' to boot a fresh instance.")

def main():
    parser = argparse.ArgumentParser(description="Manage the ESP32 Sensor Hub server.")
    parser.add_argument("action", choices=["start", "stop", "clear-data"], 
                        help="Action to perform: 'start' (starts the server), 'stop' (stops the server), 'clear-data' (wipes all history and stops the server)")
    
    args = parser.parse_args()

    if args.action == "start":
        start_server()
    elif args.action == "stop":
        stop_server()
    elif args.action == "clear-data":
        clear_data()

if __name__ == "__main__":
    main()
