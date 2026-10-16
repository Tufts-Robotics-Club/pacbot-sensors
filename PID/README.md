# pacbot-pid

Proof of concept for a PID loop that turns the SparkFun ICM-20948 IMU in 90 degree steps. Each press of the ESP32's boot button moves the setpoint 90 degrees counterclockwise (viewed from above the IMU), and the loop drives a motor until the IMU's yaw matches it.

The yaw comes from integrating the gyro's Z axis at 200 Hz. The gyro's zero offset is measured at startup, so **keep the IMU still for the first second** after reset. The magnetometer isn't used: the motor's magnet would throw it off.

## Status: no motor yet
`setMotor()` in `firmware/src/main.cpp` is empty, so the motor output is only computed and reported. You can test the loop two ways without a motor:

- **By hand** (default): press the boot button, then turn the IMU yourself. `u` (the motor output) drops toward 0 as you approach the target and flips sign if you go past it.
- **Simulated** (`sim 1`): the IMU is replaced by a model of a geared DC motor, so the full loop runs. Use this to try gains and check the step behaviour.

## Wiring
Same as the IMU project: IMU SDA to GPIO 21, SCL to GPIO 22, plus 3V3 and GND. The boot button is built into the board (GPIO 0; GPIO 9 on the ESP32-C3).

## Flash and monitor
```
pio run -d firmware -t upload
pio device monitor -d firmware
```

The firmware prints one JSON line 20 times a second: `sp` setpoint and `yaw` in degrees, `rate` in deg/s, and `u`, the motor output from -1 to 1. Lines starting with `#` are status messages, including a summary after each move:

```
# reached 90.0 in 0.64 s, overshoot 0.4 deg, final error -0.43 deg
```

## Dashboard
The dashboard in `IMU/server` switches to a PID view when this firmware is running. It plots setpoint vs angle, error, motor output and rate, lists each move's result, and has buttons and fields for the commands below. Set it up as described in `IMU/README.md`, then from `IMU/server`:

```
python server.py
```

Open http://localhost:8000. Close the serial monitor first: only one program can use the port, and the server also needs to be stopped to flash.

## Serial commands
Type these into the serial monitor and press Enter (or use the dashboard's controls):

| Command | Effect |
| --- | --- |
| `kp 0.03`, `ki 0.02`, `kd 0.002` | Set a gain |
| `min 0.1` | Smallest output that gets the motor moving |
| `step` | Same as pressing the boot button |
| `go 45` | Move to an absolute angle |
| `zero` | Call the current angle 0 |
| `cal` | Re-measure the gyro offset (keep still) |
| `sim 1` / `sim 0` | Use the simulated motor / the real IMU |
| `stream 0` / `stream 1` | Stop/resume the JSON lines, to read the move summaries |
| `show` | Print the current settings |

Gains are lost on reset. Copy values that work into the defaults at the top of `main.cpp`.

## Adding the motor
A DC motor needs an H-bridge driver (DRV8833, TB6612FNG or L298N) between it and the ESP32, with its own power supply and a ground shared with the ESP32. Fill in `setMotor(u)` to drive it: PWM with duty `|u|`, direction from the sign of `u`.

Then:

1. **Direction:** use `go` to command a small move. If the motor turns away from the target, swap its two wires.
2. **`min`:** find the smallest output that gets the motor moving from a stop.
3. **Tune:**
    - Set `ki 0` and `kd 0`, and raise `kp` until it overshoots.
    - Raise `kd` until the overshoot goes away.
    - Add `ki` back only if moves stop short of the target.
