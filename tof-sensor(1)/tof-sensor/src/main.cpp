#include <Wire.h>
#include <Arduino.h>
#include "vl53l4cd_class.h"


#define DEV_I2C Wire

constexpr int num_sensors = 4;
const int XSHUT_PIN[num_sensors] = {25, 26, 27, 14};
const uint8_t addresses[num_sensors] = {0x29, 0x54, 0x56, 0x58};
constexpr int offsets[num_sensors] = {15, 17, 26, 28};

VL53L4CD sensors[num_sensors] {
  VL53L4CD(&DEV_I2C, XSHUT_PIN[0]),
  VL53L4CD(&DEV_I2C, XSHUT_PIN[1]),
  VL53L4CD(&DEV_I2C, XSHUT_PIN[2]),
  VL53L4CD(&DEV_I2C, XSHUT_PIN[3])
};

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Starting...");

  DEV_I2C.begin(21, 22);
  DEV_I2C.setClock(100000);   // start slow; raise to 400000 once it works

  // I2C scan: the sensor should appear at 0x56 once XSHUT is high
  for (int i = 0; i < num_sensors; i++) {
    pinMode(XSHUT_PIN[i], OUTPUT);
    digitalWrite(XSHUT_PIN[i], HIGH);
  }
  
  delay(20);
  
  for (uint8_t a = 1; a < 127; a++) {
    DEV_I2C.beginTransmission(a);
    if (DEV_I2C.endTransmission() == 0) {
      Serial.printf("Found device at 0x%02X\n", a);
    }
  }

  for (int i = 0; i < num_sensors; i++) {
    sensors[i].begin();
    sensors[i].VL53L4CD_Off();
  }

  for (int i = 0; i < num_sensors; i++) {
    uint8_t s = sensors[i].InitSensor(addresses[i]); // Set the I2C address to 0x56 (default)
    Serial.printf("InitSensor status: %u\n", s);

    s = sensors[i].VL53L4CD_SetRangeTiming(200, 0);
    Serial.printf("SetRangeTiming status: %u\n", s);

    s = sensors[i].VL53L4CD_StartRanging();
    Serial.printf("StartRanging status: %u\n", s);
  }
}

void loop() {
  for (int i = 0; i < num_sensors; i++) {
    uint8_t ready = 0;
    uint32_t t0 = millis();

    do {
      sensors[i].VL53L4CD_CheckForDataReady(&ready);
      delay(5);
    } while (!ready && millis() - t0 < 1000);

    if (!ready) {
      Serial.print("Timeout for sensor ");
      Serial.println(i);
      return;
    }
  }

  for (int i = 0; i < num_sensors; i++) {
    VL53L4CD_Result_t results;
    sensors[i].VL53L4CD_ClearInterrupt();
    sensors[i].VL53L4CD_GetResult(&results);

    Serial.printf("Distance %u = ", i + 1);
    if (offsets[i] < results.distance_mm)
    Serial.printf("%u mm", results.distance_mm - offsets[i]);
    else 
      Serial.printf("0 mm");
  }
  Serial.println("");
}