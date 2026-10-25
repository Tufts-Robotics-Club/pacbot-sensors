#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float    accel[3];   // x y z in g
    float    gyro[3];    // x y z in deg/s
    int64_t  t_us;
} imu_data_t;

void imu_start(void);

// copies the newest reading into out. false if there isnt one yet
bool imu_latest(imu_data_t *out);
