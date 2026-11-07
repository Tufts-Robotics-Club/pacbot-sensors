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
#define STACK_MOTOR   4096   // sscanf/snprintf for the motor commands need more than 2048
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

// motors on the L293D. left and right are swapped from the arduino encoder test
// EN is the master on/off (low = both outputs off, motor coasts)
// PWM goes on IN1/IN2: forward = PWM on IN2 with IN1 low, backward = the other way, both low = brake
#define LEFT_EN       25   // L293D pin 1 (1,2EN), 12k pull down
#define LEFT_IN1      26   // L293D pin 2 (1A)
#define LEFT_IN2      27   // L293D pin 7 (2A)
#define RIGHT_EN      23   // L293D pin 9 (3,4EN), 12k pull down
#define RIGHT_IN1     19   // L293D pin 10 (3A)
#define RIGHT_IN2     18   // L293D pin 15 (4A)
#define MOTOR_PWM_HZ  300  // same as the arduino version, the L293D is slow so keep this low

// motor encoders, same pins as the arduino encoder test
#define LEFT_ENC_A    16
#define LEFT_ENC_B    17
#define RIGHT_ENC_A   32
#define RIGHT_ENC_B   33
#define LEFT_ENC_REVERSED  0   // set to 1 on a wheel whose count goes down when it turns forward
#define RIGHT_ENC_REVERSED 1

#define PRINT_ENC     0        // prints encoder counts to usb serial, set to 0 once its working
#define SEND_ENC      1        // sends encoder counts to the server (shows up in console.py as "robot: enc ...")
#define ENC_REPORT_HZ 5        // how often the counts get printed/sent
