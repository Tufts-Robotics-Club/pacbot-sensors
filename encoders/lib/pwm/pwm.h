#pragma once

#include <Arduino.h>

class Pwm {
public: // 5V
    Pwm(uint8_t pin, uint8_t channel, uint32_t frequency = 300, uint8_t resolution = 8)
        : pin_(pin), channel_(channel), frequency_(frequency), resolution_(resolution),
          maxDuty_((1UL << resolution) - 1) {}

    bool begin() {
        if (ledcSetup(channel_, frequency_, resolution_) == 0) return false;
        ledcAttachPin(pin_, channel_);
        return true;
    }

    void writeDuty(uint32_t duty) {
        duty = min(duty, maxDuty_);
        // Serial.printf("Duty Cycle %u%%\n", duty);
        ledcWrite(channel_, duty);   // channel, not pin
    }

    void writePercent(uint8_t percent) {
        // Serial.printf("Duty Cycle percentage %u%%\n", percent);
        percent = min(percent, static_cast<uint8_t>(100));
        writeDuty((maxDuty_ * percent) / 100);
    }

    void stop() { writeDuty(0); }

private:
    uint8_t pin_;
    uint8_t channel_;
    uint32_t frequency_;
    uint8_t resolution_;
    uint32_t maxDuty_;
};