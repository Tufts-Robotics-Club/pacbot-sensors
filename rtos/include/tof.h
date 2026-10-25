#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

typedef struct {
    uint16_t mm[NUM_TOF];
    int64_t  t_us;
} tof_data_t;

void tof_start(void);

// copies the newest reading into out. false if there isnt one yet
bool tof_latest(tof_data_t *out);
