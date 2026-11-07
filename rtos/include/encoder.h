#pragma once

#include <stdint.h>

typedef enum {
    ENC_LEFT,
    ENC_RIGHT,
} enc_side_t;

// sets up the pulse counter for both wheels. call from motor_task so its interrupt lands on core 1
void encoder_start(void);

// ticks since encoder_start(), negative when the wheel spins backward
int32_t encoder_count(enc_side_t side);
