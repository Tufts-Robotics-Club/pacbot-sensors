#include <Arduino.h>
#include <Wire.h>
#include <ICM_20948.h>

// I2C pins. SDA/SCL are the board's default pins (GPIO 21/22 on esp32dev);
// change these if the Qwiic cable is wired to different GPIOs.
#define I2C_SDA SDA
#define I2C_SCL SCL

// Boot button: GPIO 0 on esp32dev, GPIO 9 on the ESP32-C3. Reads LOW when pressed.
#if CONFIG_IDF_TARGET_ESP32C3
#define BUTTON_PIN 9
#else
#define BUTTON_PIN 0
#endif

#define LOOP_INTERVAL_US 5000   // 200 Hz control loop
#define STREAM_INTERVAL_MS 50   // 20 Hz telemetry
#define STEP_DEG 90.0f          // how far one button press turns
#define DEBOUNCE_MS 50

// The IMU counts as settled when it is this close to the setpoint and this
// still. A move is reported as done after staying settled for SETTLE_MS.
#define SETTLE_ERR_DEG 1.0f
#define SETTLE_RATE_DPS 5.0f
#define SETTLE_MS 250
#define MOVE_TIMEOUT_MS 5000

// Simulated motor (the "sim 1" command): a geared DC motor that reaches
// SIM_MAX_DPS at full output with time constant SIM_TAU_S, and doesn't move
// at all below SIM_STICTION output.
#define SIM_MAX_DPS 300.0f
#define SIM_TAU_S 0.08f
#define SIM_STICTION 0.10f

ICM_20948_I2C imu;

// Gains and settings, all changeable over serial (see handleCommand).
// Output is -1..1 (full reverse to full forward).
float kp = 0.03f;         // output per degree of error
float ki = 0.02f;         // output per degree-second of error
float kd = 0.002f;        // output per deg/s of rotation
float minOutput = 0.10f;  // smallest output that gets the motor moving
float iZone = 10.0f;      // only integrate within this many degrees of the setpoint
float iMax = 0.5f;        // limit on the integral term
bool simulate = false;
bool stream = true;

float gyroBias = 0;  // deg/s, measured at startup
float yaw = 0;       // degrees, + = counterclockwise seen from above (+Z). Not wrapped.
float rate = 0;      // deg/s, same sign as yaw
float setpoint = 0;
float integral = 0;
float output = 0;
float simRate = 0;
uint32_t lastLoopUs = 0;
uint32_t lastData = 0;

struct {
    bool active = false;
    uint32_t start;
    float from;
    float overshoot;
    uint32_t settledSince;  // 0 = not settled
} move;

// The SparkFun breakout uses address 0x69 (AD0 = 1) unless the ADR jumper is
// closed, in which case it is 0x68 (AD0 = 0). Try both.
bool connectIMU() {
    for (int ad0 = 1; ad0 >= 0; ad0--) {
        imu.begin(Wire, ad0);
        if (imu.status == ICM_20948_Stat_Ok) {
            Serial.printf("# ICM-20948 found at 0x%02X\n", 0x68 + ad0);
            break;
        }
    }
    if (imu.status != ICM_20948_Stat_Ok) {
        Serial.printf("# ICM-20948 not found: %s\n", imu.statusString());
        return false;
    }

    // The library defaults to +-250 deg/s, which a motor can easily exceed.
    ICM_20948_fss_t fss;
    fss.a = gpm2;
    fss.g = dps1000;
    // Its digital low-pass filter is off by default; ~120 Hz cuts noise
    // without adding noticeable lag at a 200 Hz loop.
    ICM_20948_dlpcfg_t dlpf;
    dlpf.a = acc_d473bw_n499bw;
    dlpf.g = gyr_d119bw5_n154bw3;
    if (imu.setFullScale(ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr, fss) != ICM_20948_Stat_Ok ||
        imu.setDLPFcfg(ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr, dlpf) != ICM_20948_Stat_Ok ||
        imu.enableDLPF(ICM_20948_Internal_Gyr, true) != ICM_20948_Stat_Ok) {
        Serial.printf("# ICM-20948 config failed: %s\n", imu.statusString());
        return false;
    }
    return true;
}

// Drives the motor: -1..1, + turns toward +yaw. No motor driver is connected
// yet, so this does nothing; the output is still reported over serial.
void setMotor(float u) {
}

// Averages the gyro at rest to find its zero offset, which would otherwise
// make the angle drift.
void calibrateGyro() {
    Serial.println("# calibrating gyro, keep the IMU still");
    setMotor(0);
    float sum = 0;
    int n = 0;
    uint32_t start = millis();
    while (millis() - start < 1500) {
        if (imu.dataReady()) {
            imu.getAGMT();
            // Skip the first 500 ms: right after power-on and configuration
            // the gyro reads tens of deg/s off.
            if (millis() - start >= 500) {
                sum += imu.gyrZ();
                n++;
            }
        }
        delay(5);
    }
    gyroBias = n ? sum / n : 0;
    Serial.printf("# gyro Z bias %.3f deg/s (%d samples)\n", gyroBias, n);
    lastLoopUs = micros();  // don't count the calibration time as one long loop step
}

void printSettings() {
    Serial.printf("# kp %.4f  ki %.4f  kd %.4f  min %.3f  sim %d  stream %d\n",
                  kp, ki, kd, minOutput, simulate, stream);
}

void moveTo(float target) {
    setpoint = target;
    integral = 0;
    move.active = true;
    move.start = millis();
    move.from = yaw;
    move.overshoot = 0;
    move.settledSince = 0;
    Serial.printf("# moving to %.1f\n", target);
}

void resetAngle() {
    yaw = 0;
    setpoint = 0;
    integral = 0;
    simRate = 0;
    move.active = false;
}

// One command per line, e.g. "kp 0.05". Typing an unknown command lists them.
void handleCommand(String line) {
    line.trim();
    if (line.length() == 0) {
        return;
    }
    int space = line.indexOf(' ');
    String cmd = space < 0 ? line : line.substring(0, space);
    bool hasValue = space >= 0;
    float value = hasValue ? line.substring(space + 1).toFloat() : 0;

    if (cmd == "kp" && hasValue) kp = value;
    else if (cmd == "ki" && hasValue) ki = value;
    else if (cmd == "kd" && hasValue) kd = value;
    else if (cmd == "min" && hasValue) minOutput = constrain(value, 0.0f, 1.0f);
    else if (cmd == "stream" && hasValue) stream = value != 0;
    else if (cmd == "sim" && hasValue) {
        simulate = value != 0;
        resetAngle();
    }
    else if (cmd == "step") { moveTo(setpoint + STEP_DEG); return; }
    else if (cmd == "go" && hasValue) { moveTo(value); return; }
    else if (cmd == "zero") { resetAngle(); Serial.println("# angle reset to 0"); return; }
    else if (cmd == "cal") { calibrateGyro(); return; }
    else if (cmd != "show") {
        Serial.println("# commands: kp <x>, ki <x>, kd <x>, min <x>, step, go <deg>, "
                       "zero, cal, sim <0|1>, stream <0|1>, show");
        return;
    }
    printSettings();
}

void pollSerial() {
    static String buffer;
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            handleCommand(buffer);
            buffer = "";
        } else if (buffer.length() < 64) {
            buffer += c;
        }
    }
}

// True once per press of the boot button.
bool buttonPressed() {
    static bool wasDown = false;
    static uint32_t lastChange = 0;
    bool down = digitalRead(BUTTON_PIN) == LOW;
    uint32_t now = millis();
    if (down == wasDown || now - lastChange < DEBOUNCE_MS) {
        return false;
    }
    wasDown = down;
    lastChange = now;
    return down;
}

// Updates rate and yaw, from the gyro or from the simulated motor.
void updateAngle(float dt, uint32_t now) {
    if (simulate) {
        float drive = fabsf(output) < SIM_STICTION ? 0 : output;
        simRate += (SIM_MAX_DPS * drive - simRate) / SIM_TAU_S * dt;
        rate = simRate;
    } else if (imu.dataReady()) {
        imu.getAGMT();
        rate = imu.gyrZ() - gyroBias;
        lastData = now;
    } else if (now - lastData > 1000) {
        // Recover if the sensor was unplugged or lost power. The angle is no
        // longer trustworthy after this, so stop wherever we are.
        Serial.println("# no data from ICM-20948, reconnecting");
        output = 0;
        setMotor(0);
        rate = 0;
        connectIMU();
        setpoint = yaw;
        move.active = false;
        lastData = now;
    }
    yaw += rate * dt;
}

bool isSettled() {
    return fabsf(setpoint - yaw) < SETTLE_ERR_DEG && fabsf(rate) < SETTLE_RATE_DPS;
}

void updatePID(float dt) {
    if (isSettled()) {
        output = 0;  // close enough: stop instead of buzzing around the target
        return;
    }

    float error = setpoint - yaw;
    // Integrating only near the target keeps the integral from winding up
    // during the big initial swing of a 90 degree move.
    if (fabsf(error) < iZone) {
        integral = constrain(integral + ki * error * dt, -iMax, iMax);
    }
    // The D term uses the gyro rate directly: it is the true angular velocity,
    // so there's no need to differentiate the (noisier) angle.
    float u = constrain(kp * error + integral - kd * rate, -1.0f, 1.0f);

    // Rescale so any nonzero command is strong enough to overcome stiction.
    if (u != 0) {
        u = copysignf(minOutput + fabsf(u) * (1 - minOutput), u);
    }
    output = u;
}

// Reports how each move went, for tuning.
void trackMove(uint32_t now) {
    if (!move.active) {
        return;
    }
    float direction = setpoint >= move.from ? 1 : -1;
    move.overshoot = max(move.overshoot, (yaw - setpoint) * direction);

    if (!isSettled()) {
        move.settledSince = 0;
    } else if (move.settledSince == 0) {
        move.settledSince = now;
    }

    if (move.settledSince && now - move.settledSince >= SETTLE_MS) {
        Serial.printf("# reached %.1f in %.2f s, overshoot %.1f deg, final error %.2f deg\n",
                      setpoint, (move.settledSince - move.start) / 1000.0f,
                      move.overshoot, setpoint - yaw);
        move.active = false;
    } else if (now - move.start > MOVE_TIMEOUT_MS) {
        Serial.printf("# did not settle at %.1f after %d s, error %.2f deg, overshoot %.1f deg\n",
                      setpoint, MOVE_TIMEOUT_MS / 1000, setpoint - yaw, move.overshoot);
        move.active = false;
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    setMotor(0);

    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);

    while (!connectIMU()) {
        delay(1000);
    }
    calibrateGyro();
    lastData = millis();
    printSettings();
}

void loop() {
    pollSerial();
    if (buttonPressed()) {
        moveTo(setpoint + STEP_DEG);
    }

    uint32_t nowUs = micros();
    if (nowUs - lastLoopUs < LOOP_INTERVAL_US) {
        return;
    }
    float dt = (nowUs - lastLoopUs) * 1e-6f;
    lastLoopUs = nowUs;
    uint32_t now = millis();

    updateAngle(dt, now);
    updatePID(dt);
    setMotor(output);
    trackMove(now);

    static uint32_t lastStream = 0;
    if (stream && now - lastStream >= STREAM_INTERVAL_MS) {
        lastStream = now;
        // sp/yaw in degrees, rate in deg/s, u = motor output -1..1.
        Serial.printf("{\"t\":%lu,\"sp\":%.1f,\"yaw\":%.2f,\"rate\":%.1f,\"u\":%.3f}\n",
                      (unsigned long)now, setpoint, yaw, rate, output);
    }
}
