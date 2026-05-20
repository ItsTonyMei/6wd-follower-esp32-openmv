#include "MotorTask.h"
#include <esp_task_wdt.h>

void MotorTask::begin(MotorDriver* motor, UltrasonicSensors* ultrasonic,
                       DataAggregator* aggregator, QueueHandle_t cmdQueue) {
    motor_      = motor;
    ultrasonic_ = ultrasonic;
    aggregator_ = aggregator;
    cmdQueue_   = cmdQueue;
}

// FreeRTOS task 入口，每 5ms 循环一次
void MotorTask::taskFunc(void* param) {
    auto* self = static_cast<MotorTask*>(param);
    esp_task_wdt_add(NULL);
    for (;;) {
        esp_task_wdt_reset();
        self->run();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void MotorTask::run() {
    const unsigned long now = millis();

    // Step 1: 超声波测距更新
    ultrasonic_->update();
    UltrasonicReadings us = ultrasonic_->readings();
    uint8_t blockedSide = ultrasonic_->obstacleSide();

    // Step 2: 非阻塞读取 MotorCmd queue
    MotorCmd cmd = {CarCmd::STOP, 0};
    bool hasCmd = (xQueueReceive(cmdQueue_, &cmd, 0) == pdPASS);
    if (hasCmd) {
        lastCmdMs_ = now;
        commandWatchdogActive_ = true;
    }

    // Step 3: 危险区 (<20cm) → 强制 STOP
    if (checkDanger(us)) {
        return;
    }

    // Step 4: 命令链路未建立前保持 STOP
    if (!commandWatchdogActive_) {
        motor_->stop();
        currentAction_ = "STOP";
        leftPwm_ = rightPwm_ = 0;
        reportState();
        return;
    }

    // Step 5: 命令超时看门狗 (>500ms 无命令 → STOP)
    if (checkCommandTimeout(now)) {
        return;
    }

    // Step 6: 警告区 (20-40cm) — 避障优先于视觉命令
    if (blockedSide != US_SIDE_NONE) {
        if (!avoidanceActive_) {
            avoidanceActive_ = true;
            avoidanceStartMs_ = now;
        }

        bool stillBlocked = checkStillBlocked(us, blockedSide);
        if (!stillBlocked) {
            avoidanceActive_ = false;
        } else if ((now - avoidanceStartMs_) > AVOIDANCE_TIMEOUT_MS) {
            // 避障超时 → 后退策略
            doAvoidanceReverse();
            return;
        } else {
            doAvoidance(blockedSide);
            return;
        }
    } else {
        avoidanceActive_ = false;
    }

    // Step 7: 正常执行 FollowLogic 下发的 MotorCmd
    if (hasCmd) {
        executeMotorCmd(cmd);
    }

    // Step 8: 更新 aggregator（供 Web Dashboard 轮询）
    reportState();
}

bool MotorTask::checkDanger(const UltrasonicReadings& us) {
    bool leftSensorOK  = (us.leftCm  >= 0 && us.leftCm  < ULTRASONIC_MAX_CM);
    bool rightSensorOK = (us.rightCm >= 0 && us.rightCm < ULTRASONIC_MAX_CM);
    bool dangerLeft    = leftSensorOK  && (us.leftCm  < OBSTACLE_MIN_CM);
    bool dangerRight   = rightSensorOK && (us.rightCm < OBSTACLE_MIN_CM);

    if (dangerLeft || dangerRight) {
        motor_->stop();
        currentAction_ = "STOP";
        leftPwm_ = rightPwm_ = 0;
        reportState();
        return true;
    }
    return false;
}

bool MotorTask::checkCommandTimeout(unsigned long now) {
    if (commandWatchdogActive_ && (now - lastCmdMs_ > COMMAND_TIMEOUT_MS)) {
        motor_->stop();
        currentAction_ = "STOP";
        leftPwm_ = rightPwm_ = 0;
        reportState();
        return true;
    }
    return false;
}

// 检查障碍物是否仍然存在（防止单次误检提前退出避障）
bool MotorTask::checkStillBlocked(const UltrasonicReadings& us, uint8_t blockedSide) {
    if (blockedSide == US_SIDE_LEFT  && us.leftCm  > OBSTACLE_WARN_CM) return false;
    if (blockedSide == US_SIDE_RIGHT && us.rightCm > OBSTACLE_WARN_CM) return false;
    if (blockedSide == US_SIDE_BOTH  && (us.leftCm > OBSTACLE_WARN_CM && us.rightCm > OBSTACLE_WARN_CM)) return false;
    return true;
}

// 避障转向：远离障碍物一侧减速，另一侧保持速度
void MotorTask::doAvoidance(uint8_t blockedSide) {
    if (blockedSide == US_SIDE_LEFT) {
        // 左侧有障碍 → 向右转
        motor_->drive(AVOIDANCE_SPEED, AVOIDANCE_SPEED / 3);
        currentAction_ = "AVD_RGT";
        leftPwm_ = AVOIDANCE_SPEED;
        rightPwm_ = AVOIDANCE_SPEED / 3;
    } else if (blockedSide == US_SIDE_RIGHT) {
        // 右侧有障碍 → 向左转
        motor_->drive(AVOIDANCE_SPEED / 3, AVOIDANCE_SPEED);
        currentAction_ = "AVD_LFT";
        leftPwm_ = AVOIDANCE_SPEED / 3;
        rightPwm_ = AVOIDANCE_SPEED;
    } else {
        // 双侧有障碍 → 后退
        motor_->reverse(AVOIDANCE_SPEED);
        currentAction_ = "AVD_REV";
        leftPwm_ = rightPwm_ = AVOIDANCE_SPEED;
    }
    reportState();
}

// 避障超时 → 后退策略
void MotorTask::doAvoidanceReverse() {
    motor_->reverse(AVOIDANCE_SPEED / 2);
    currentAction_ = "AVD_REV_T";
    leftPwm_ = rightPwm_ = AVOIDANCE_SPEED / 2;
    reportState();
}

void MotorTask::reportState() {
    if (aggregator_->lock(pdMS_TO_TICKS(5))) {
        aggregator_->updateCar(buildCarState());
        aggregator_->unlock();
    }
}

void MotorTask::executeMotorCmd(const MotorCmd& cmd) {
    switch (cmd.cmd) {
        case CarCmd::STOP:
            motor_->stop();
            currentAction_ = "STOP";
            leftPwm_ = rightPwm_ = 0;
            break;
        case CarCmd::FWD: {
            int pwm = constrain(cmd.pwm, 0, PWM_RANGE);
            motor_->forward(pwm);
            currentAction_ = "FWD";
            leftPwm_ = rightPwm_ = pwm;
            break;
        }
        case CarCmd::REV: {
            int pwm = constrain(cmd.pwm, 0, PWM_RANGE);
            motor_->reverse(pwm);
            currentAction_ = "REV";
            leftPwm_ = rightPwm_ = pwm;
            break;
        }
        case CarCmd::LFT: {
            int pwm = constrain(cmd.pwm, 0, PWM_RANGE);
            motor_->rotateLeft(pwm);
            currentAction_ = "LFT";
            leftPwm_ = 0;
            rightPwm_ = pwm;
            break;
        }
        case CarCmd::RGT: {
            int pwm = constrain(cmd.pwm, 0, PWM_RANGE);
            motor_->rotateRight(pwm);
            currentAction_ = "RGT";
            leftPwm_ = pwm;
            rightPwm_ = 0;
            break;
        }
    }
}

CarState MotorTask::buildCarState() const {
    UltrasonicReadings us = ultrasonic_->readings();
    CarState s;
    s.valid = true;
    s.leftPwm = leftPwm_;
    s.rightPwm = rightPwm_;
    s.leftUltrasonic = us.leftCm;
    s.rightUltrasonic = us.rightCm;
    safeStrCopy(s.action, currentAction_, sizeof(s.action));
    s.timestamp = millis();
    return s;
}
