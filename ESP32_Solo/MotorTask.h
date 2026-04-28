#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "Config.h"
#include "MotorDriver.h"
#include "UltrasonicSensors.h"
#include "DataAggregator.h"

class MotorTask {
public:
    void begin(MotorDriver* motor, UltrasonicSensors* ultrasonic,
               DataAggregator* aggregator, SemaphoreHandle_t mutex,
               QueueHandle_t cmdQueue);

    // FreeRTOS task entry point
    static void taskFunc(void* param);

private:
    void run();
    void executeMotorCmd(const MotorCmd& cmd);
    CarState buildCarState() const;
    void reportState();
    bool checkDanger(const UltrasonicReadings& us);
    bool checkCommandTimeout(unsigned long now);
    bool checkStillBlocked(const UltrasonicReadings& us, uint8_t blockedSide);
    void doAvoidance(uint8_t blockedSide);
    void doAvoidanceReverse();

    MotorDriver*        motor_;
    UltrasonicSensors*  ultrasonic_;
    DataAggregator*     aggregator_;
    SemaphoreHandle_t   mutex_;
    QueueHandle_t       cmdQueue_;

    // Obstacle avoidance state
    bool         avoidanceActive_  = false;
    unsigned long avoidanceStartMs_ = 0;

    // Command watchdog
    unsigned long lastCmdMs_          = 0;
    bool         commandWatchdogActive_ = false;

    // Current motor state for broadcasting
    int         leftPwm_         = 0;
    int         rightPwm_        = 0;
    const char* currentAction_   = "STOP";

    bool lockAggregator(TickType_t waitTicks);
    void unlockAggregator();
};
