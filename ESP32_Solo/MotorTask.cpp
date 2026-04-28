#include "MotorTask.h"
#include "ProtocolUtils.h"
#include <esp_task_wdt.h>

void MotorTask::begin(MotorDriver* motor, UltrasonicSensors* ultrasonic,
                       DataAggregator* aggregator, SemaphoreHandle_t mutex,
                       QueueHandle_t cmdQueue) {
    motor_      = motor;
    ultrasonic_ = ultrasonic;
    aggregator_ = aggregator;
    mutex_      = mutex;
    cmdQueue_   = cmdQueue;
}

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

    // Step 1: update ultrasonic readings
    ultrasonic_->update();
    UltrasonicReadings us = ultrasonic_->readings();
    uint8_t blockedSide = ultrasonic_->obstacleSide();

    // Step 2: non-blocking read from command queue
    MotorCmd cmd = {CarCmd::STOP, 0};
    bool hasCmd = (xQueueReceive(cmdQueue_, &cmd, 0) == pdPASS);
    if (hasCmd) {
        lastCmdMs_ = now;
        commandWatchdogActive_ = true;
    }

    // Step 3: danger zone (< 20cm) forces immediate stop
    if (checkDanger(us)) {
        return;
    }

    // Step 4: before command link is alive, stay stopped
    if (!commandWatchdogActive_) {
        motor_->stop();
        currentAction_ = "STOP";
        leftPwm_ = rightPwm_ = 0;
        reportState();
        return;
    }

    // Step 5: command link watchdog — stop if no command for 500ms
    if (checkCommandTimeout(now)) {
        return;
    }

    // Step 6: warning zone (20-40cm) — avoidance takes priority over vision commands
    if (blockedSide != US_SIDE_NONE) {
        if (!avoidanceActive_) {
            avoidanceActive_ = true;
            avoidanceStartMs_ = now;
        }

        bool stillBlocked = checkStillBlocked(us, blockedSide);
        if (!stillBlocked) {
            avoidanceActive_ = false;
        } else if ((now - avoidanceStartMs_) > AVOIDANCE_TIMEOUT_MS) {
            doAvoidanceReverse();
            return;
        } else {
            doAvoidance(blockedSide);
            return;
        }
    } else {
        avoidanceActive_ = false;
    }

    // Step 7: execute vision command (from FollowLogic via queue)
    if (hasCmd) {
        executeMotorCmd(cmd);
    }

    // Step 8: update aggregator
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

bool MotorTask::checkStillBlocked(const UltrasonicReadings& us, uint8_t blockedSide) {
    if (blockedSide == US_SIDE_LEFT  && us.leftCm  > OBSTACLE_WARN_CM) return false;
    if (blockedSide == US_SIDE_RIGHT && us.rightCm > OBSTACLE_WARN_CM) return false;
    if (blockedSide == US_SIDE_BOTH  && (us.leftCm > OBSTACLE_WARN_CM && us.rightCm > OBSTACLE_WARN_CM)) return false;
    return true;
}

void MotorTask::doAvoidance(uint8_t blockedSide) {
    if (blockedSide == US_SIDE_LEFT) {
        motor_->drive(AVOIDANCE_SPEED, AVOIDANCE_SPEED / 3);
        currentAction_ = "AVD_RGT";
        leftPwm_ = AVOIDANCE_SPEED;
        rightPwm_ = AVOIDANCE_SPEED / 3;
    } else if (blockedSide == US_SIDE_RIGHT) {
        motor_->drive(AVOIDANCE_SPEED / 3, AVOIDANCE_SPEED);
        currentAction_ = "AVD_LFT";
        leftPwm_ = AVOIDANCE_SPEED / 3;
        rightPwm_ = AVOIDANCE_SPEED;
    } else {
        motor_->reverse(AVOIDANCE_SPEED);
        currentAction_ = "AVD_REV";
        leftPwm_ = rightPwm_ = AVOIDANCE_SPEED;
    }
    reportState();
}

void MotorTask::doAvoidanceReverse() {
    motor_->reverse(AVOIDANCE_SPEED / 2);
    currentAction_ = "AVD_REV_T";
    leftPwm_ = rightPwm_ = AVOIDANCE_SPEED / 2;
    reportState();
}

void MotorTask::reportState() {
    if (lockAggregator(pdMS_TO_TICKS(5))) {
        aggregator_->updateCar(buildCarState());
        unlockAggregator();
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

bool MotorTask::lockAggregator(TickType_t waitTicks) {
    if (mutex_ == nullptr) return true;
    return xSemaphoreTake(mutex_, waitTicks) == pdTRUE;
}

void MotorTask::unlockAggregator() {
    if (mutex_ == nullptr) return;
    xSemaphoreGive(mutex_);
}
