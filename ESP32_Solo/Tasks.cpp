#include "Tasks.h"
#include "Config.h"
#include "VisionBridge.h"
#include "FollowLogic.h"
#include "DataAggregator.h"
#include "DashboardServer.h"
#include <esp_task_wdt.h>

// ============================================================================
// 外部引用（定义于 ESP32_Solo.ino）
// External references (defined in ESP32_Solo.ino)
// ============================================================================
extern VisionBridge       visionBridge;
extern FollowLogic        followLogic;
extern DataAggregator     aggregator;
extern QueueHandle_t      motorCmdQueue;
extern DashboardServer   webServer;

// ============================================================================
// VisionTask: OpenMV UART1 接收 → FollowLogic 决策 → MotorCmd queue
// VisionTask: reads OpenMV VIS data via UART1, runs FollowLogic, sends MotorCmd
// ============================================================================
void visionTaskFunc(void* param) {
    Serial.println("[VisionTask] started");
    esp_task_wdt_add(NULL);

    for (;;) {
        esp_task_wdt_reset();

        // 读取 UART1，解析 VIS 协议帧
        visionBridge.handle();

        if (visionBridge.hasValidReading()) {
            // 更新 VisState 到 aggregator（供 Web Dashboard 轮询）
            if (aggregator.lock(pdMS_TO_TICKS(10))) {
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
                aggregator.unlock();
            }

            // FollowLogic 决策 → MotorCmd → 发送到 MotorTask queue
            MotorCmd mc = followLogic.update(
                visionBridge.hasPerson(),
                visionBridge.cx(),
                visionBridge.feetY(),
                visionBridge.distScore()
            );

            xQueueSend(motorCmdQueue, &mc, pdMS_TO_TICKS(10));
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ============================================================================
// WebTask: WiFi AP + HTTP Dashboard (Core 1, pri 1)
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
