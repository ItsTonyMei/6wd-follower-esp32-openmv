// ============================================================================
// 履带车视觉跟随系统 — ESP32 L2 决策层 (Phase 0.2 Clean)
// MCU: ESP32-WROOM-32U (DevKit V1), Xtensa LX6 双核 @ 240MHz, Flash 4MB
//
// 已移除: UltrasonicSensors, MotorDriver, MotorTask (PWM直驱 → STM32)
// 保留:   VisionBridge, FollowLogic, DataAggregator, DashboardServer
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

// ============================================================
// 全局对象
// ============================================================

DataAggregator      aggregator;
DashboardServer    webServer;
VisionBridge       visionBridge;
FollowLogic        followLogic;

// FreeRTOS IPC
QueueHandle_t      motorCmdQueue;

// ============================================================
// Task Handles
// ============================================================

static TaskHandle_t logicTaskHandle = nullptr;
static TaskHandle_t webTaskHandle   = nullptr;

// ============================================================
// MainTask: VIS 解析 + FollowLogic + MotorCmd 输出 (Core 0)
// ============================================================

void mainTaskFunc(void* param) {
    Serial.println("[MainTask] started");
    esp_task_wdt_add(NULL);

    for (;;) {
        esp_task_wdt_reset();

        // 读取 OpenMV VIS 帧 (SoftSerial GPIO18)
        visionBridge.handle();

        if (visionBridge.hasValidReading()) {
            // 更新 VisState 到 DataAggregator
            if (aggregator.lock(pdMS_TO_TICKS(10))) {
                VisState vs;
                vs.valid       = visionBridge.hasValidReading();
                vs.hasPerson   = visionBridge.hasPerson();
                vs.cx          = visionBridge.cx();
                vs.cy          = visionBridge.cy();
                vs.w           = visionBridge.w();
                vs.h           = visionBridge.h();
                vs.confidence  = visionBridge.confidence();
                vs.type[0]     = '\0';
                strncpy(vs.type, visionBridge.type(), sizeof(vs.type) - 1);
                vs.distScore   = visionBridge.distScore();
                vs.tofDistance = visionBridge.tofDistance();
                vs.feetY       = visionBridge.feetY();
                vs.timestamp   = millis();
                aggregator.updateVis(vs);
                aggregator.unlock();
            }

            // FollowLogic 决策 → MotorCmd
            MotorCmd mc = followLogic.update(
                visionBridge.hasPerson(),
                visionBridge.cx(),
                visionBridge.feetY(),
                visionBridge.distScore()
            );

            // TODO(Phase 3): MotorCmd → UART2 → STM32
            // 当前 Phase 0.2 仅推入 queue 验证数据流
            xQueueSend(motorCmdQueue, &mc, pdMS_TO_TICKS(10));
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ============================================================
// WebTask: WiFi AP + HTTP Dashboard (Core 1)
// ============================================================

void webTaskFunc(void* param) {
    Serial.println("[WebTask] started");
    esp_task_wdt_add(NULL);

    for (;;) {
        esp_task_wdt_reset();
        webServer.handle();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ============================================================
// Setup
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n====================================");
    Serial.println("ESP32 L2 — Phase 0.2 Clean Boot");
    Serial.println("====================================");

    // VisionBridge (SoftSerial GPIO18 RX ← OpenMV)
    visionBridge.begin();

    // DataAggregator + 互斥锁
    SemaphoreHandle_t mtx = xSemaphoreCreateMutex();
    if (mtx == nullptr) {
        Serial.println("[ERROR] Mutex creation failed!");
        esp_restart();
    }
    aggregator.begin(mtx);

    // WiFi AP
    Serial.print("WiFi AP: ");
    Serial.print(WIFI_SSID);
    if (!WiFi.softAP(WIFI_SSID, WIFI_PASS)) {
        Serial.println(" ... FAILED");
    } else {
        Serial.print(" ... OK  IP: ");
        Serial.println(WiFi.softAPIP());
    }
    delay(200);

    // MotorCmd FreeRTOS queue
    motorCmdQueue = xQueueCreate(4, sizeof(MotorCmd));
    if (motorCmdQueue == nullptr) {
        Serial.println("[ERROR] Queue creation failed!");
        esp_restart();
    }

    // Web Dashboard
    webServer.begin(&aggregator);

    // ─── 创建 FreeRTOS Tasks ───

    // MainTask: VIS + FollowLogic (Core 0, priority 3)
    BaseType_t res = xTaskCreatePinnedToCore(
        mainTaskFunc, "MainTask", TASK_STACK_SIZE,
        nullptr, 3, &logicTaskHandle, 0);
    if (res != pdPASS) Serial.println("[ERROR] MainTask creation failed!");

    // WebTask: Dashboard (Core 1, priority 1)
    res = xTaskCreatePinnedToCore(
        webTaskFunc, "WebTask", TASK_STACK_SIZE,
        nullptr, 1, &webTaskHandle, 1);
    if (res != pdPASS) Serial.println("[ERROR] WebTask creation failed!");

    Serial.println();
    Serial.println("=== System Ready ===");
    Serial.print("  WiFi: "); Serial.print(WIFI_SSID);
    Serial.print(" @ "); Serial.println(WiFi.softAPIP());
    Serial.println("  Dashboard: http://192.168.4.1");
    Serial.println("  VIS: waiting for OpenMV...");
    Serial.println("  MotorCmd: queue ready (STM32 not connected)");
    Serial.println("====================\n");

    // 删除 loop task, 仅保留 FreeRTOS tasks
    vTaskDelete(NULL);
}

void loop() {}  // never reached
