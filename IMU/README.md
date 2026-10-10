# pacbot-imu

Reads a SparkFun Qwiic 9DoF IMU (ICM-20948) from an ESP32 over I2C and shows the data live in a browser.

The ESP32 prints one JSON line per sample over USB serial (50 Hz). A Python server on the computer reads the serial port and serves a page on localhost that plots the accelerometer, gyroscope and magnetometer.

## Wiring
| IMU | ESP32 |
| --- | --- |
| 3V3 | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

These are the default I2C pins of the `esp32dev` board. For other pins, change `I2C_SDA` / `I2C_SCL` in `firmware/src/main.cpp`.

## USB driver
The ESP32 must show up as a COM port. Boards with a CP2102 chip need the [CP210x driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) on Windows.

## Setting up the Python server
From this folder (PowerShell):

```
cd server
python -m venv .venv
.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
```

## Flash the ESP32
Open the `firmware` directory in VS Code and use PlatformIO to build and upload, or from the command line:

```
pio run -d firmware -t upload
```

## Start the server
Close any serial monitor first (only one program can use the port), then:

```
python server.py
```

Open http://localhost:8000. The serial port is auto-detected; pass `--port COM5` to pick one. Set `IMU_PORT` to change the web port.
