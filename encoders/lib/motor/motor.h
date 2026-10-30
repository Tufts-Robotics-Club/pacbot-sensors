#pragma once

#include <pwm.h>

// Two motors on an L293D: each has a PWM enable pin (speed) and two
// direction pins (IN1/IN2). Forward = DIR1 LOW, DIR2 HIGH
class Motor {
public:
    Pwm left_motor;
    Pwm right_motor;

    bool leftMotorForward = true;
    bool rightMotorForward = true;

    Motor(Pwm &leftPwm, Pwm &rightPwm,
          uint8_t leftDir1, uint8_t leftDir2, uint8_t rightDir1, uint8_t rightDir2)
        : left_motor(leftPwm), right_motor(rightPwm),
          leftDir1_(leftDir1), leftDir2_(leftDir2), rightDir1_(rightDir1), rightDir2_(rightDir2) {}

    void begin() {
        pinMode(leftDir1_, OUTPUT);
        pinMode(leftDir2_, OUTPUT);
        pinMode(rightDir1_, OUTPUT);
        pinMode(rightDir2_, OUTPUT);
        left_motor.begin();
        right_motor.begin();
        stop();
        switchDirection();
    }

    void stop() {
        left_motor.writePercent(0);
        right_motor.writePercent(0);
    }

    void moveForward(int leftSpeed, int rightSpeed) {
        if (!leftMotorForward || !rightMotorForward) {
            leftMotorForward = true;
            rightMotorForward = true;
            this->switchDirection();
        }
        left_motor.writePercent(leftSpeed);
        right_motor.writePercent(rightSpeed);
    }

    void moveBackward(int leftSpeed, int rightSpeed){
        if (leftMotorForward || rightMotorForward) {
            leftMotorForward = false;
            rightMotorForward = false;
            this->switchDirection();
        }
        left_motor.writePercent(leftSpeed);
        right_motor.writePercent(rightSpeed);
    }

    void turn(int speedLeft, int speedRight) {
        left_motor.writePercent(speedLeft);
        right_motor.writePercent(speedRight);
    }

    void pivotCW(int speed) {
        leftMotorForward = true;
        rightMotorForward = false;
        this -> switchDirection();

        left_motor.writePercent(speed);
        right_motor.writePercent(speed);
    }

    void pivotCCW(int speed) {
        leftMotorForward = false;
        rightMotorForward = true;
        this -> switchDirection();

        left_motor.writePercent(speed);
        right_motor.writePercent(speed);
    }

    // signed duty cycle per wheel (in terms of percentage), -100..100 (%)
    // negative = backward; postive = forward
    void setDuty(int leftDuty, int rightDuty) {
        leftDuty = constrain(leftDuty, -100, 100);
        rightDuty = constrain(rightDuty, -100, 100);

        leftMotorForward = leftDuty >= 0;
        rightMotorForward = rightDuty >= 0;
        this->switchDirection();

        left_motor.writePercent(abs(leftDuty));
        right_motor.writePercent(abs(rightDuty));
    }

private:
    uint8_t leftDir1_;
    uint8_t leftDir2_;
    uint8_t rightDir1_;
    uint8_t rightDir2_;

    void switchDirection() {
        this->switchDirectionLeft();
        this->switchDirectionRight();
    }

    void switchDirectionLeft() {
        if (leftMotorForward) {
            digitalWrite(leftDir1_, LOW);
            digitalWrite(leftDir2_, HIGH);
        } else {
            digitalWrite(leftDir1_, HIGH);
            digitalWrite(leftDir2_, LOW);
        }
    }

    void switchDirectionRight() {
        if (rightMotorForward) {
            digitalWrite(rightDir1_, LOW);
            digitalWrite(rightDir2_, HIGH);
        } else {
            digitalWrite(rightDir1_, HIGH);
            digitalWrite(rightDir2_, LOW);
        }
    }
};
