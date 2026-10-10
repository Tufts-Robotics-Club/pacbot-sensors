# pacbot-websockets

An implementation of websocket communication between the game server and client robot.

## Setting up the Python server
From root:

```
cd server
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
```

## Start the server
Run the server with a host address accessible to the ESP32:
`WS_HOST=0.0.0.0 WS_PORT=8765 python server.py`

## Flash the ESP32
Open the firmware directory in VS Code, connect the ESP32 over USB, and use PlatformIO to build and upload the firmware.
Open the PlatformIO Serial Monitor to view connection status and received messages.