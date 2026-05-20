#pragma once

#include <Arduino.h>

// ============================================================================
// MotorDriver: LEDC PWM 电机驱动 (RZ7886 H-bridge)
// MotorDriver: LEDC PWM motor controller for RZ7886 dual H-bridge
// ============================================================================
class MotorDriver {
public:
    void begin();
    void stop();

    // 底层驱动: 直接控制左右轮功率 (-255..+255)
    // Low-level: direct left/right power control
    void drive(int leftPower, int rightPower);

    // 语义命令 (Semantic commands)
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
