# pacbot-websockets

An implementation of websocket communication between the game server and client robot.

## Setting up the Python server
From root:

```sh
cd server
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
```

## Start the Python server

From the repository root:

```sh
cd server
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
python bridge.py

```

## Flash the ESP32
1. Open the firmware directory in VS Code, connect the ESP32 over USB, and use PlatformIO to build and upload the firmware.
2. Open the PlatformIO Serial Monitor to view connection status and received messages.

## Simulate the client with Wokwi
1. Open the client directory in VS Code with the PlatformIO and Wokwi extensions installed.
2. Start the server using the steps above.
3. Build the Wokwi firmware environment:
```sh
pio run -e esp32dev-wokwi
```
4. Start the simulation with Wokwi: Start Simulator from the VS Code Command Palette.
5. Check the Wokwi serial output for the connection and registration messages.

The Wokwi firmware connects to `host.wokwi.internal:8765`. The project’s wokwi.toml points the network gateway to `ws://localhost:9011`; make sure the Wokwi gateway is running and can reach the server on port 8765.