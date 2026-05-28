#pragma once
#include <Arduino.h>

// ============================================================================
// 履带车视觉跟随系统 — ESP8266 配置 (替代 ESP32)
// 所有引脚、阈值、参数的单一数据源
// ============================================================================
//
// 架构: OpenMV (L1感知) → ESP8266 (L2决策+WiFi) → STM32 (L3执行+安全)
// ESP8266 替代 ESP32 全部功能: VIS接收 + FollowLogic + Dashboard + STM32通信
//
// 供电: 48V 89Ah 锂电池 → 48V→5V 10A 防水降压 → ESP8266 (USB)

// ─── 引脚分配 ───
// ┌──────────────────┬────────────────┬─────────────────────────────┐
// │  功能             │ 引脚            │ 目标                         │
// ├──────────────────┼────────────────┼─────────────────────────────┤
// │ VIS RX (SW Ser)  │ D5 / GPIO14    │ ← OpenMV P0 SW UART @ 4800  │
// │ STM32 TX (UART0) │ D8 / GPIO15    │ → STM32 PB11 (USART3 RX)    │
// │ STM32 RX (UART0) │ D7 / GPIO13    │ ← STM32 PB10 (USART3 TX)    │
// └──────────────────┴────────────────┴─────────────────────────────┘
// UART0 通过 Serial.swap() 重映射到 GPIO15(TX)/GPIO13(RX)
// 原 UART0 引脚 GPIO1(TX)/GPIO3(RX) 释放为普通 GPIO

// ─── VIS 接收 (SoftwareSerial) ───
constexpr uint8_t PIN_VIS_RX    = 14;   // D5 ← OpenMV P0

// ─── STM32 通信 (UART0 swapped) ───
// Serial.swap() 后: TX=GPIO15(D8), RX=GPIO13(D7)
constexpr uint8_t PIN_STM32_TX  = 15;   // D8 → STM32 PB11
constexpr uint8_t PIN_STM32_RX  = 13;   // D7 ← STM32 PB10
constexpr uint32_t STM32_BAUD   = 115200;

// ─── WiFi AP ───
constexpr char WIFI_SSID[] = "Rover";
constexpr char WIFI_PASS[] = "12345678";

// ─── HC6060A 混控款电调 PWM 参数 ───
// 白线=油门, 黄线=转向, 电调内部处理差速混控
// 50Hz 舵机 PWM: 1000-2000μs, 中位 1500μs
constexpr uint16_t PWM_NEUTRAL         = 1500;
constexpr uint16_t PWM_MIN             = 1000;
constexpr uint16_t PWM_MAX             = 2000;
constexpr uint16_t MAX_THROTTLE_OFFSET = 400;
constexpr uint16_t MAX_STEER_OFFSET    = 300;
constexpr uint16_t THROTTLE_DEADBAND   = 20;
constexpr float    TURN_KP             = 2.0f;
constexpr int      CX_MIN_OFFSET       = 15;

// ─── 时序参数 ───
constexpr uint32_t STM32_CMD_INTERVAL_MS = 50;
constexpr uint32_t CMD_TIMEOUT_MS        = 500;
constexpr uint32_t VISION_TIMEOUT_MS     = 700;
constexpr uint32_t ESC_INIT_DELAY_MS     = 3000;

// ─── MotorCmd ───
struct MotorCmd {
    uint16_t throttle;  // 白线油门 (1000-2000μs, 1500=停止)
    uint16_t steering;  // 黄线转向 (1000-2000μs, 1500=直行)
};
