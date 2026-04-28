#include "MotorDriver.h"
#include "Config.h"

// Dead zone time to prevent H-bridge shoot-through when direction changes
static constexpr unsigned long DEAD_TIME_MS = 5;

void MotorDriver::begin() {
    _lastLeftPower  = 0;
    _lastRightPower = 0;

    // ESP32 LEDC setup — 4 pins for 4 motor directions
    // ESP32 Arduino core 3.x API: ledcAttach(pin, freq, resolution)
    ledcAttach(PIN_LEFT_FORWARD,  PWM_FREQ_HZ, PWM_RESOLUTION);
    ledcAttach(PIN_LEFT_REVERSE,  PWM_FREQ_HZ, PWM_RESOLUTION);
    ledcAttach(PIN_RIGHT_FORWARD, PWM_FREQ_HZ, PWM_RESOLUTION);
    ledcAttach(PIN_RIGHT_REVERSE, PWM_FREQ_HZ, PWM_RESOLUTION);

    stop();
}

void MotorDriver::stop() {
    ledcWrite(PIN_LEFT_FORWARD,  0);
    ledcWrite(PIN_LEFT_REVERSE,  0);
    ledcWrite(PIN_RIGHT_FORWARD, 0);
    ledcWrite(PIN_RIGHT_REVERSE, 0);
}

void MotorDriver::drive(int leftPower, int rightPower) {
    applyDeadZone(leftPower, rightPower);
    writeMotor(PIN_LEFT_FORWARD,  PIN_LEFT_REVERSE,  leftPower);
    writeMotor(PIN_RIGHT_FORWARD, PIN_RIGHT_REVERSE, rightPower);
}

void MotorDriver::applyDeadZone(int leftPower, int rightPower) {
    bool leftDirChanged  = ((_lastLeftPower  > 0) != (leftPower  > 0)) ||
                           ((_lastLeftPower  < 0) != (leftPower  < 0));
    bool rightDirChanged = ((_lastRightPower > 0) != (rightPower > 0)) ||
                           ((_lastRightPower < 0) != (rightPower < 0));

    if (leftDirChanged || rightDirChanged) {
        stop();
        delay(DEAD_TIME_MS);
    }

    _lastLeftPower  = leftPower;
    _lastRightPower = rightPower;
}

void MotorDriver::forward(int power) {
    power = constrain(power, 0, PWM_RANGE);
    drive(power, power);
}

void MotorDriver::reverse(int power) {
    power = constrain(power, 0, PWM_RANGE);
    drive(-power, -power);
}

void MotorDriver::rotateLeft(int power) {
    power = constrain(power, 0, PWM_RANGE);
    drive(-power, power);
}

void MotorDriver::rotateRight(int power) {
    power = constrain(power, 0, PWM_RANGE);
    drive(power, -power);
}

void MotorDriver::writeMotor(uint8_t fwdPin, uint8_t revPin, int power) {
    power = constrain(power, -PWM_RANGE, PWM_RANGE);

    if (power > 0) {
        ledcWrite(fwdPin, power);
        ledcWrite(revPin, 0);
    } else if (power < 0) {
        ledcWrite(fwdPin, 0);
        ledcWrite(revPin, -power);
    } else {
        ledcWrite(fwdPin, 0);
        ledcWrite(revPin, 0);
    }
}
