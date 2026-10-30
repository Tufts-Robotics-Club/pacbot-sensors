#pragma once

#include <Arduino.h>
#include <ESP32Encoder.h>

// Quadrature encoder (Adafruit N20 magnetic encoder), counted in hardware by
// the ESP32's pulse counter so no ticks are missed at high speed.
// Counts every edge of A and B, so one wheel turn = COUNTS_PER_REV ticks.
// Same interface as the Blues Clues interrupt version
class Encoder {
public:
    Encoder(uint8_t pinA, uint8_t pinB, bool reversed = false)
        : pinA_(pinA), pinB_(pinB), reversed_(reversed) {}

    void begin() {
        ESP32Encoder::useInternalWeakPullResistors = puType::up;
        enc_.attachFullQuad(pinA_, pinB_);
        enc_.clearCount();
    }

    // ticks since begin() or the last reset(), negative when spinning backward
    int32_t count() {
        int32_t c = (int32_t)enc_.getCount();
        return reversed_ ? -c : c;
    }

    void reset() { enc_.clearCount(); }

private:
    uint8_t pinA_;
    uint8_t pinB_;
    bool reversed_;
    ESP32Encoder enc_;
};
