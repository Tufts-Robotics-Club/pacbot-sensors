#pragma once

#include <Arduino.h>
#include <motor.h>
#include <encoder.h>

//HOLDS each wheel at a target speed (encoder counts per second) by adjusting
//the motor duty cycle with the encoders. Call update() every loop()
class Drive {
public:
    Drive(Motor &motors, Encoder &leftEnc, Encoder &rightEnc)
        : motors_(motors), leftEnc_(leftEnc), rightEnc_(rightEnc) {}

    // targets in counts/s (COUNTS_PER_REV counts = 1 wheel turn), negative = backward
    void setWheelSpeeds(int leftTarget, int rightTarget) {
        if (active_ && leftTarget == leftTarget_ && rightTarget == rightTarget_) return;  // already set
        active_ = true;
        leftTarget_ = leftTarget;
        rightTarget_ = rightTarget;
        // start from a guess so the wheels don't have to ramp up from 0
        leftDuty_ = feedforward(leftTarget);
        rightDuty_ = feedforward(rightTarget);
    }

    void stop() { setWheelSpeeds(0, 0); }

    // stop controlling the motors (for open-loop duty tests); speeds are still measured
    void release() { active_ = false; }

    void update() {
        unsigned long now = millis(); //get current time
        if (now - lastUpdate_ < UPDATE_MS) return; //if it's been 20 ms
        float dt = (now - lastUpdate_) / 1000.0f; //convert elapsed time to seconds
        lastUpdate_ = now; //restart elapsed time timer

        // measured speed = change in count / time
        long left = leftEnc_.count(); //left encoder ticks
        long right = rightEnc_.count(); //right encoder ticks
        leftSpeed_ = (left - lastLeft_) / dt; //ticks / elapsed time
        rightSpeed_ = (right - lastRight_) / dt;
        lastLeft_ = left; //re sample ticks to current tick count
        lastRight_ = right;

        if (!active_) return;

        // adjust each duty in whichever direction closes the speed error
        if (leftTarget_ != 0)
            leftDuty_ = constrain(leftDuty_ + KI * (leftTarget_ - leftSpeed_) * dt, -100.0f, 100.0f);
        if (rightTarget_ != 0)
            rightDuty_ = constrain(rightDuty_ + KI * (rightTarget_ - rightSpeed_) * dt, -100.0f, 100.0f);

        motors_.setDuty(lroundf(leftDuty_), lroundf(rightDuty_));
    }

    float leftSpeed() const { return leftSpeed_; }    // measured, counts/s
    float rightSpeed() const { return rightSpeed_; }

private:
    static constexpr unsigned long UPDATE_MS = 20;  // control loop runs at 50 Hz
    // PLACEHOLDERS for the N20 1:100 + L293D: measure MAX_SPEED with "d 100",
    // then retune KI (Blues Clues used 2000 and 0.05 for the Pololu motors)
    static constexpr float MAX_SPEED = 9000.0f;     // counts/s at 100% duty
    static constexpr float KI = 0.01f;              // duty % added per count of accumulated error
    static constexpr float MIN_DUTY = 40.0f;        // % where the motor starts turning (measured ~100/255)

    // starting duty guess: MIN_DUTY to get past the deadband, plus the rest of
    // the range scaled by how fast we want to go
    static float feedforward(int target) {
        if (target == 0) return 0;
        float duty = MIN_DUTY + abs(target) * (100.0f - MIN_DUTY) / MAX_SPEED;
        return constrain(target > 0 ? duty : -duty, -100.0f, 100.0f);
    }

    Motor &motors_;
    Encoder &leftEnc_;
    Encoder &rightEnc_;

    bool active_ = false;
    int leftTarget_ = 0;
    int rightTarget_ = 0;
    float leftDuty_ = 0;
    float rightDuty_ = 0;
    float leftSpeed_ = 0;
    float rightSpeed_ = 0;
    long lastLeft_ = 0;
    long lastRight_ = 0;
    unsigned long lastUpdate_ = 0;
};