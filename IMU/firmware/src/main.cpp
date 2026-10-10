#include <Arduino.h>
#include <Wire.h>
#include <ICM_20948.h>

// I2C pins. SDA/SCL are the board's default pins (GPIO 21/22 on esp32dev);
// change these if the Qwiic cable is wired to different GPIOs.
#define I2C_SDA SDA
#define I2C_SCL SCL

#define SAMPLE_INTERVAL_MS 20  // 50 Hz

ICM_20948_I2C imu;

// The SparkFun breakout uses address 0x69 (AD0 = 1) unless the ADR jumper is
// closed, in which case it is 0x68 (AD0 = 0). Try both.
bool connectIMU() {
    for (int ad0 = 1; ad0 >= 0; ad0--) {
        imu.begin(Wire, ad0);
        if (imu.status == ICM_20948_Stat_Ok) {
            Serial.printf("# ICM-20948 found at 0x%02X\n", 0x68 + ad0);
            return true;
        }
    }
    Serial.printf("# ICM-20948 not found: %s\n", imu.statusString());
    return false;
}

void setup() {
    Serial.begin(115200);

    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);

    while (!connectIMU()) {
        delay(1000);
    }
}

void loop() {
    static uint32_t lastSample = 0;
    static uint32_t lastData = 0;

    uint32_t now = millis();
    if (now - lastSample < SAMPLE_INTERVAL_MS) {
        return;
    }
    lastSample = now;

    if (!imu.dataReady()) {
        // Recover if the sensor was unplugged or lost power.
        if (now - lastData > 1000) {
            Serial.println("# no data from ICM-20948, reconnecting");
            connectIMU();
            lastData = now;
        }
        return;
    }
    lastData = now;

    imu.getAGMT();

    // One JSON object per line: accel in mg, gyro in deg/s, mag in uT, temp in C.
    Serial.printf(
        "{\"t\":%lu,\"a\":[%.1f,%.1f,%.1f],\"g\":[%.2f,%.2f,%.2f],"
        "\"m\":[%.1f,%.1f,%.1f],\"temp\":%.1f}\n",
        (unsigned long)now,
        imu.accX(), imu.accY(), imu.accZ(),
        imu.gyrX(), imu.gyrY(), imu.gyrZ(),
        imu.magX(), imu.magY(), imu.magZ(),
        imu.temp()
    );
}
