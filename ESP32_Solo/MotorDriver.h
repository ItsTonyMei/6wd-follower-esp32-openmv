#pragma once

#include <Arduino.h>

class MotorDriver {
public:
    void begin();
    void stop();

    // low-level: direct motor control (left/right power, -255..+255)
    void drive(int leftPower, int rightPower);

    // semantic commands
    void forward(int power);
    void reverse(int power);
    void rotateLeft(int power);
    void rotateRight(int power);

private:
    void writeMotor(uint8_t fwdPin, uint8_t revPin, int power);
    void applyDeadZone(int leftPower, int rightPower);

    int _lastLeftPower  = 0;
    int _lastRightPower = 0;
};
