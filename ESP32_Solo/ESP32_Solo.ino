// ============================================================================
// 6轮车-UniBoard: ESP32 单板控制方案
// ESP32 统一控制电机、超声波、视觉跟随、WiFi Dashboard
// OpenMV 仅负责摄像头 + YOLO 推理，通过 UART1 发送 VIS 数据
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
#include "MotorDriver.h"
#include "UltrasonicSensors.h"
#include "MotorTask.h"
#include "Tasks.h"

// ============================================================
// Shared Resources
// ============================================================

DataAggregator      aggregator;
DashboardServer    webServer;
VisionBridge       visionBridge;
FollowLogic        followLogic;
MotorDriver        motor;
UltrasonicSensors  ultrasonic;
MotorTask          motorTask;

QueueHandle_t      motorCmdQueue;
SemaphoreHandle_t   aggregatorMutex;

// ============================================================
// Task Handles
// ============================================================

static TaskHandle_t motorTaskHandle  = nullptr;
static TaskHandle_t visionTaskHandle = nullptr;
static TaskHandle_t webTaskHandle    = nullptr;

// ============================================================
// Setup
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(500);

    // Init subsystems
    aggregator.begin();
    motor.begin();
    ultrasonic.begin();
    motor.stop();
    visionBridge.begin();

    // WiFi AP — phone connects directly
    if (!WiFi.softAP(WIFI_SSID, WIFI_PASS)) {
        Serial.println("[ERROR] WiFi AP creation failed!");
    }
    delay(200);

    // Create FreeRTOS primitives
    motorCmdQueue = xQueueCreate(4, sizeof(MotorCmd));
    if (motorCmdQueue == nullptr) {
        Serial.println("[ERROR] Failed to create motorCmdQueue!");
        esp_restart();
    }

    aggregatorMutex = xSemaphoreCreateMutex();
    if (aggregatorMutex == nullptr) {
        Serial.println("[ERROR] Failed to create aggregatorMutex!");
        esp_restart();
    }

    webServer.begin(&aggregator, aggregatorMutex);

    // Init MotorTask
    motorTask.begin(&motor, &ultrasonic, &aggregator, aggregatorMutex, motorCmdQueue);

    Serial.println();
    Serial.println("=== 6轮车-UniBoard: ESP32 单板控制 ===");
    Serial.println("OpenMV P4(TX)  -> GPIO15 (UART1 RX)");
    Serial.println("RZ7886 A-1     <- GPIO25 (LEDC PWM)");
    Serial.println("RZ7886 A-2     <- GPIO26 (LEDC PWM)");
    Serial.println("RZ7886 B-1     <- GPIO27 (LEDC PWM)");
    Serial.println("RZ7886 B-2     <- GPIO19 (LEDC PWM)");
    Serial.println("HC-SR04 L      -> GPIO32/34");
    Serial.println("HC-SR04 R      -> GPIO33/35");
    Serial.print("WiFi AP: ");
    Serial.print(WIFI_SSID);
    Serial.print("  IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("Open http://192.168.4.1 in browser");
    Serial.println();

    // Create FreeRTOS tasks
    BaseType_t res;

    res = xTaskCreatePinnedToCore(
        MotorTask::taskFunc, "MotorTask", TASK_STACK_SIZE, &motorTask, 3, &motorTaskHandle, 0);
    if (res != pdPASS) Serial.println("[ERROR] MotorTask creation failed!");

    res = xTaskCreatePinnedToCore(
        visionTaskFunc, "VisionTask", TASK_STACK_SIZE, nullptr, 3, &visionTaskHandle, 0);
    if (res != pdPASS) Serial.println("[ERROR] VisionTask creation failed!");

    res = xTaskCreatePinnedToCore(
        webTaskFunc, "WebTask", TASK_STACK_SIZE, nullptr, 1, &webTaskHandle, 1);
    if (res != pdPASS) Serial.println("[ERROR] WebTask creation failed!");

    Serial.println("[setup] All tasks created. Deleting main loop task.");
    vTaskDelete(NULL);
}

void loop() {
    vTaskDelete(NULL);
}
