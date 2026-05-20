#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "Config.h"
#include "MotorDriver.h"
#include "UltrasonicSensors.h"
#include "DataAggregator.h"

// ============================================================================
// MotorTask: 电机控制 + 避障状态机（FreeRTOS task，Core 0，优先级 3）
// MotorTask: motor control + obstacle avoidance state machine
// ============================================================================
class MotorTask {
public:
    void begin(MotorDriver* motor, UltrasonicSensors* ultrasonic,
               DataAggregator* aggregator, QueueHandle_t cmdQueue);

    // FreeRTOS task 入口
    static void taskFunc(void* param);

private:
    void run();

    // 避障优先级链:
    // Obstacle avoidance priority chain:
    //   1. 危险区 (<20cm) → STOP
    //   2. 命令链路未建立 → STOP
    //   3. 命令超时 (>500ms) → STOP
    //   4. 警告区 (20-40cm) → 避障转向 / 后退
    //   5. 正常 → 执行 VisionTask 下发的 MotorCmd
    void executeMotorCmd(const MotorCmd& cmd);
    CarState buildCarState() const;
    void reportState();
    bool checkDanger(const UltrasonicReadings& us);
    bool checkCommandTimeout(unsigned long now);
    bool checkStillBlocked(const UltrasonicReadings& us, uint8_t blockedSide);
    void doAvoidance(uint8_t blockedSide);
    void doAvoidanceReverse();

    MotorDriver*        motor_         = nullptr;
    UltrasonicSensors*  ultrasonic_    = nullptr;
    DataAggregator*     aggregator_    = nullptr;
    QueueHandle_t       cmdQueue_      = nullptr;

    // 避障状态
    bool         avoidanceActive_    = false;
    unsigned long avoidanceStartMs_  = 0;

    // 命令看门狗
    unsigned long lastCmdMs_             = 0;
    bool         commandWatchdogActive_  = false;

    // 当前电机状态（用于 Web Dashboard 广播）
    int         leftPwm_       = 0;
    int         rightPwm_      = 0;
    const char* currentAction_ = "STOP";
};
