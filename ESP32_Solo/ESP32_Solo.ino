// ============================================================================
// 履带车视觉跟随系统 — ESP32 L2 决策层
// MCU: ESP32-WROOM-32U (DevKit V1), ESP32-D0WD-V3 rev3.1, 240MHz, 4MB Flash
// HC6060A 混控款电调: throttle/steering PWM 脉宽经 STM32 转发
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
static TaskHandle_t motorTaskHandle = nullptr;
static TaskHandle_t webTaskHandle   = nullptr;

// ─── CRC8 (XOR-based, same as VIS protocol) ───
static uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : crc << 1;
    }
    return crc;
}

// ─── 发送 MotorCmd 到 STM32 (下行协议) ───
// Frame: [0xAA] [throttle_lo] [throttle_hi] [steering_lo] [steering_hi] [CRC8]
static void sendMotorCmd(const MotorCmd& mc) {
    uint8_t buf[6];
    buf[0] = 0xAA;
    buf[1] = mc.throttle & 0xFF;
    buf[2] = (mc.throttle >> 8) & 0xFF;
    buf[3] = mc.steering & 0xFF;
    buf[4] = (mc.steering >> 8) & 0xFF;
    buf[5] = crc8(&buf[1], 4);
    Serial2.write(buf, 6);
}

// ─── MainTask: VIS 接收 + FollowLogic 决策 (Core 0) ───
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

// ─── MotorTask: MotorCmd 消费 + Serial2 下行发送 (Core 0) ───
void motorTaskFunc(void* param) {
    Serial.println("[MotorTask] started");
    esp_task_wdt_add(NULL);

    Serial2.begin(STM32_UART_BAUD, SERIAL_8N1, PIN_STM32_RX, PIN_STM32_TX);
    MotorCmd mc = {PWM_NEUTRAL, PWM_NEUTRAL};

    // 上电先发送中位信号，等 STM32 + ESC 自检完成
    for (int i = 0; i < 60; i++) {  // 60 × 50ms = 3s
        sendMotorCmd(mc);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    Serial.println("[MotorTask] ESC init complete, running");

    for (;;) {
        esp_task_wdt_reset();

        // 排空 queue，只保留最新一条
        MotorCmd tmp;
        while (xQueueReceive(motorCmdQueue, &tmp, 0)) {
            mc = tmp;
        }

        sendMotorCmd(mc);

        // 更新 CarState 供 Dashboard 显示
        if (aggregator.lock(pdMS_TO_TICKS(10))) {
            CarState cs;
            cs.valid     = true;
            cs.throttle  = mc.throttle;
            cs.steering  = mc.steering;
            cs.timestamp = millis();
            aggregator.updateCar(cs);
            aggregator.unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(STM32_CMD_INTERVAL_MS));
    }
}

// ─── WebTask: WiFi Dashboard HTTP server (Core 1) ───
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
    Serial.println("ESP32 L2 — HC6060A Mixed-Mode ESC");
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

    xTaskCreatePinnedToCore(mainTaskFunc,  "MainTask",  TASK_STACK_SIZE, nullptr, 3, &logicTaskHandle, 0);
    xTaskCreatePinnedToCore(motorTaskFunc, "MotorTask", TASK_STACK_SIZE, nullptr, 2, &motorTaskHandle, 0);
    xTaskCreatePinnedToCore(webTaskFunc,   "WebTask",   TASK_STACK_SIZE, nullptr, 1, &webTaskHandle,  1);

    Serial.println("=== System Ready ===");
    Serial.print("  WiFi: "); Serial.print(WIFI_SSID);
    Serial.print(" @ "); Serial.println(WiFi.softAPIP());
    Serial.println("  Dashboard: http://192.168.4.1");
    Serial.print("  STM32 UART2: GPIO"); Serial.print(PIN_STM32_TX);
    Serial.print("/GPIO"); Serial.print(PIN_STM32_RX);
    Serial.print(" @ "); Serial.print(STM32_UART_BAUD); Serial.println(" baud");
    Serial.println("  VIS: waiting for OpenMV...");
    Serial.println("====================\n");
    vTaskDelete(NULL);
}

void loop() {}
