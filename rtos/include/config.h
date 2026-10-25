#pragma once

// wifi name, password and the laptops ip live in secrets.h so they dont end up on github
// copy secrets.example.h to secrets.h and fill it in
#include "secrets.h"

#define SERVER_PORT      8765     // same port server.py listens on
#define WS_RECONNECT_MS  3000     // same 3 s retry as the club's arduino client

// wifi already runs on core 0 so our wifi stuff goes there too
// core 1 is only for the robot
#define COMMS_CORE    0
#define RT_CORE       1

// higher number wins (wifi is 23 on core 0)
#define PRIO_MOTOR    22
#define PRIO_IMU      21
#define PRIO_TOF      20
#define PRIO_REPLY    5
#define PRIO_IMU_SEND 4
#define PRIO_STATS    1

// stack sizes are in bytes on the esp32 not words
// sized off what stats_task measured plus room for the real sensor and motor code
#define STACK_MOTOR   2048
#define STACK_TOF     3072
#define STACK_IMU     3072
#define STACK_REPLY   2048
#define STACK_STATS   3072   // printing floats takes more stack
#define STACK_IMU_SEND 3072  // same reason

#define MOTOR_HZ      1000
#define TOF_HZ        100
#define IMU_HZ        200

// i2c bus for our sensors
#define I2C_SDA       21
#define I2C_SCL       22
#define I2C_HZ        400000   // fastest the imu can do over i2c

#define IMU_ADDR      0x69     // sparkfun qwiic icm-20948 (0x68 if the ADR jumper is closed)

#define PRINT_IMU     1        // prints imu readings 5 times a second, set to 0 once its working

#define SEND_IMU      1        // sends imu readings to the server
#define IMU_SEND_HZ   50       // same 50 Hz as the club's imu code

#define MSG_LEN       128      // longest message to or from the computer (imu json is ~70)

#define NUM_TOF       4        // one wall sensor per side
