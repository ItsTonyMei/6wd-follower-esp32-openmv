#include "Tasks.h"
#include "Config.h"
#include "ProtocolUtils.h"
#include "VisionBridge.h"
#include "FollowLogic.h"
#include "DataAggregator.h"
#include "DashboardServer.h"
#include <esp_task_wdt.h>
#include <cstring>

// ============================================================================
// External references (defined in ESP32_Solo.ino)
// ============================================================================
extern VisionBridge       visionBridge;
extern FollowLogic        followLogic;
extern DataAggregator     aggregator;
extern QueueHandle_t      motorCmdQueue;
extern SemaphoreHandle_t  aggregatorMutex;
extern DashboardServer   webServer;

// ============================================================================
// Helper: lock/unlock aggregator (consistent with MotorTask pattern)
// ============================================================================
static bool lockAggregator(TickType_t waitTicks) {
    return xSemaphoreTake(aggregatorMutex, waitTicks) == pdTRUE;
}
static void unlockAggregator() {
    xSemaphoreGive(aggregatorMutex);
}

// ============================================================================
// Parse FollowLogic command string → MotorCmd struct
// ============================================================================
static MotorCmd parseFollowCmd(const char* cmd) {
    if (strcmp(cmd, "STOP") == 0) return {CarCmd::STOP, 0};

    const char* colon = strchr(cmd, ':');
    if (!colon) return {CarCmd::STOP, 0};

    int pwm = atoi(colon + 1);
    if (strncmp(cmd, "FWD", 3) == 0) return {CarCmd::FWD, pwm};
    if (strncmp(cmd, "REV", 3) == 0) return {CarCmd::REV, pwm};
    if (strncmp(cmd, "LFT", 3) == 0) return {CarCmd::LFT, pwm};
    if (strncmp(cmd, "RGT", 3) == 0) return {CarCmd::RGT, pwm};

    return {CarCmd::STOP, 0};
}

// ============================================================================
// VisionTask: OpenMV UART1 + FollowLogic
// ============================================================================
void visionTaskFunc(void* param) {
    Serial.println("[VisionTask] started");
    esp_task_wdt_add(NULL);

    for (;;) {
        esp_task_wdt_reset();

        visionBridge.handle();

        if (visionBridge.hasValidReading()) {
            if (lockAggregator(pdMS_TO_TICKS(10))) {
                VisState vs;
                vs.valid = visionBridge.hasValidReading();
                vs.hasPerson = visionBridge.hasPerson();
                vs.cx = visionBridge.cx();
                vs.cy = visionBridge.cy();
                vs.w = visionBridge.w();
                vs.h = visionBridge.h();
                vs.confidence = visionBridge.confidence();
                vs.type[0] = '\0';
                safeStrCopy(vs.type, visionBridge.type(), sizeof(vs.type));
                vs.distScore = visionBridge.distScore();
                vs.feetY = visionBridge.feetY();
                vs.timestamp = millis();
                aggregator.updateVis(vs);
                unlockAggregator();
            }

            const char* cmd = followLogic.update(
                visionBridge.hasPerson(),
                visionBridge.cx(),
                visionBridge.feetY(),
                visionBridge.distScore()
            );

            MotorCmd mc = parseFollowCmd(cmd);
            xQueueSend(motorCmdQueue, &mc, pdMS_TO_TICKS(10));
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ============================================================================
// WebTask: WiFi AP + Dashboard
// ============================================================================
void webTaskFunc(void* param) {
    Serial.println("[WebTask] started");
    esp_task_wdt_add(NULL);

    for (;;) {
        esp_task_wdt_reset();
        webServer.handle();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}