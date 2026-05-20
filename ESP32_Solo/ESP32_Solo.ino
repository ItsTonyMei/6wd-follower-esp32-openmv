// ============================================================================
// 6WD Follower — ESP32 单板控制方案 (UniBoard)
// ESP32 统一控制：电机 PWM (LEDC)、超声波 HC-SR04 ×2、视觉跟随、WiFi Dashboard
// OpenMV 仅负责摄像头 + YOLO person detection，通过 UART1 单向发送 VIS 数据
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
    Serial.println("=== 6WD Follower — ESP32 UniBoard ===");
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
