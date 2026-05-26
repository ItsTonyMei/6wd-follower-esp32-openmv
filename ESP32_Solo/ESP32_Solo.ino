// ============================================================================
// 履带车视觉跟随系统 — ESP32 L2 决策层
// MCU: ESP32-WROOM-32U (DevKit V1), Xtensa LX6 双核 @ 240MHz, Flash 4MB
// ESP32 负责: OpenMV VIS 帧解析 + FollowLogic 决策 + CRSF 遥控 + WiFi Dashboard
// 电机控制通过 UART2 → STM32 (MotorCmd)，不再由 ESP32 直驱 PWM。
// 超声波 HC-SR04 已移除，距离测量由 OpenMV VL53L1X ToF 测距扩展板提供。
// TODO(Phase 4.1): 移除 UltrasonicSensors / MotorTask 超声波依赖
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
// 全局对象 (Global objects)
// ============================================================

DataAggregator      aggregator;
DashboardServer    webServer;
VisionBridge       visionBridge;
FollowLogic        followLogic;
MotorDriver        motor;
UltrasonicSensors  ultrasonic;
MotorTask          motorTask;

// FreeRTOS IPC
QueueHandle_t      motorCmdQueue;

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

    // 初始化子系统 (Init subsystems)
    motor.begin();
    ultrasonic.begin();
    motor.stop();
    visionBridge.begin();

    // 创建互斥锁并注入 DataAggregator（封装线程安全）
    SemaphoreHandle_t mtx = xSemaphoreCreateMutex();
    if (mtx == nullptr) {
        Serial.println("[ERROR] Failed to create aggregator mutex!");
        esp_restart();
    }
    aggregator.begin(mtx);

    // WiFi AP — 手机/PC 直连 Dashboard
    if (!WiFi.softAP(WIFI_SSID, WIFI_PASS)) {
        Serial.println("[ERROR] WiFi AP creation failed!");
    }
    delay(200);

    // 创建 MotorCmd FreeRTOS queue (4 元素, 每元素 2 字节)
    motorCmdQueue = xQueueCreate(4, sizeof(MotorCmd));
    if (motorCmdQueue == nullptr) {
        Serial.println("[ERROR] Failed to create motorCmdQueue!");
        esp_restart();
    }

    // 启动 Web Dashboard（HTTP port 80）
    webServer.begin(&aggregator);

    // 初始化 MotorTask（依赖注入）
    motorTask.begin(&motor, &ultrasonic, &aggregator, motorCmdQueue);

    Serial.println();
    Serial.println("=== Tracked Vehicle Follower — ESP32 L2 ===");
    Serial.println("OpenMV UART3 TX → SoftSerial GPIO18 (VIS)");
    Serial.println("ELRS CRSF       → UART1 RX=GPIO15 TX=GPIO4");
    Serial.println("STM32           ↔ UART2 TX=GPIO17 RX=GPIO16");
    Serial.println("VL53L1X ToF     → OpenMV I2C Shield (距离替代超声波)");
    Serial.print("WiFi AP: ");
    Serial.print(WIFI_SSID);
    Serial.print("  IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("Open http://192.168.4.1 in browser");
    Serial.println();

    // 创建 FreeRTOS Tasks
    BaseType_t res;

    // MotorTask: 避障 + 电机控制 (Core 0, priority 3)
    res = xTaskCreatePinnedToCore(
        MotorTask::taskFunc, "MotorTask", TASK_STACK_SIZE, &motorTask, 3, &motorTaskHandle, 0);
    if (res != pdPASS) Serial.println("[ERROR] MotorTask creation failed!");

    // VisionTask: OpenMV UART1 解析 + FollowLogic 决策 (Core 0, priority 3)
    res = xTaskCreatePinnedToCore(
        visionTaskFunc, "VisionTask", TASK_STACK_SIZE, nullptr, 3, &visionTaskHandle, 0);
    if (res != pdPASS) Serial.println("[ERROR] VisionTask creation failed!");

    // WebTask: WiFi AP + HTTP Dashboard (Core 1, priority 1)
    res = xTaskCreatePinnedToCore(
        webTaskFunc, "WebTask", TASK_STACK_SIZE, nullptr, 1, &webTaskHandle, 1);
    if (res != pdPASS) Serial.println("[ERROR] WebTask creation failed!");

    Serial.println("[setup] All tasks created. Deleting main loop task.");
    vTaskDelete(NULL);
}

// loop() 不会执行（setup 中已删除 loop task）
// loop() is never reached (setup deletes the loop task)
void loop() {}
