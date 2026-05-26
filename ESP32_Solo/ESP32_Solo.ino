// ============================================================================
// 履带车视觉跟随系统 — ESP32 L2 决策层 (Phase 0.2 Clean)
// MCU: ESP32-WROOM-32U (DevKit V1), ESP32-D0WD-V3 rev3.1, 240MHz, 4MB Flash
// 稳定基线: WiFi AP + Dashboard + VisionBridge (VIS 接收)
// ============================================================================

#include <WiFi.h>
#include <esp_task_wdt.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "Config.h"
#include "DashboardServer.h"
#include "DataAggregator.h"
#include "VisionBridge.h"
#include "FollowLogic.h"

DataAggregator    aggregator;
DashboardServer  webServer;
VisionBridge     visionBridge;
FollowLogic      followLogic;
QueueHandle_t    motorCmdQueue;

static TaskHandle_t logicTaskHandle = nullptr;
static TaskHandle_t webTaskHandle   = nullptr;

void mainTaskFunc(void* param) {
    Serial.println("[MainTask] started");
    esp_task_wdt_add(NULL);
    for (;;) {
        esp_task_wdt_reset();
        visionBridge.handle();
        if (visionBridge.hasValidReading()) {
            if (aggregator.lock(pdMS_TO_TICKS(10))) {
                VisState vs;
                vs.valid       = visionBridge.hasValidReading();
                vs.hasPerson   = visionBridge.hasPerson();
                vs.cx          = visionBridge.cx();
                vs.cy          = visionBridge.cy();
                vs.w           = visionBridge.w();
                vs.h           = visionBridge.h();
                vs.confidence  = visionBridge.confidence();
                strncpy(vs.type, visionBridge.type(), sizeof(vs.type) - 1);
                vs.distScore   = visionBridge.distScore();
                vs.tofDistance = visionBridge.tofDistance();
                vs.feetY       = visionBridge.feetY();
                vs.timestamp   = millis();
                aggregator.updateVis(vs);
                aggregator.unlock();
            }
            MotorCmd mc = followLogic.update(
                visionBridge.hasPerson(), visionBridge.cx(),
                visionBridge.feetY(), visionBridge.distScore());
            xQueueSend(motorCmdQueue, &mc, pdMS_TO_TICKS(10));
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void webTaskFunc(void* param) {
    Serial.println("[WebTask] started");
    esp_task_wdt_add(NULL);
    for (;;) {
        esp_task_wdt_reset();
        webServer.handle();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n====================================");
    Serial.println("ESP32 L2 — Phase 0.2 Stable");
    Serial.println("====================================");

    visionBridge.begin();
    SemaphoreHandle_t mtx = xSemaphoreCreateMutex();
    aggregator.begin(mtx);

    Serial.print("WiFi AP: "); Serial.print(WIFI_SSID);
    WiFi.softAP(WIFI_SSID, WIFI_PASS);
    Serial.print("  IP: "); Serial.println(WiFi.softAPIP());
    delay(200);

    motorCmdQueue = xQueueCreate(4, sizeof(MotorCmd));
    webServer.begin(&aggregator);

    xTaskCreatePinnedToCore(mainTaskFunc, "MainTask", TASK_STACK_SIZE, nullptr, 3, &logicTaskHandle, 0);
    xTaskCreatePinnedToCore(webTaskFunc, "WebTask", TASK_STACK_SIZE, nullptr, 1, &webTaskHandle, 1);

    Serial.println("=== System Ready ===");
    Serial.print("  WiFi: "); Serial.print(WIFI_SSID);
    Serial.print(" @ "); Serial.println(WiFi.softAPIP());
    Serial.println("  Dashboard: http://192.168.4.1");
    Serial.println("  VIS: waiting for OpenMV...");
    Serial.println("====================\n");
    vTaskDelete(NULL);
}

void loop() {}
