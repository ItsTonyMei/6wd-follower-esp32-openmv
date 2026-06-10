#pragma once
#include <Arduino.h>

// ============================================================================
// 履带车视觉跟随系统 — ESP8266 备用控制器配置
// 所有引脚、阈值、参数的单一数据源
// ============================================================================
//
// 架构: OpenMV (L1感知) → ESP32 (L2决策+WiFi, 主) 或 ESP8266 (备用) → STM32 (L3执行+安全)
// ESP8266 为下位备用硬件: VIS接收 + FollowLogic + Dashboard + STM32通信 (算法/协议与 ESP32 一致)
//
// 供电: 48V 89Ah 锂电池 → 48V→5V 10A 防水降压 → ESP8266 (USB)
//
// 动力: 两台三相无刷电机 + 双路独立无刷 ESC
// STM32 端坦克混控: left = thr + (st-PWM_NEUTRAL), right = thr - (st-PWM_NEUTRAL)

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
constexpr char WIFI_SSID[] = "Tracked Robot";
constexpr char WIFI_PASS[] = "12345678";

// ─── 双路无刷电调 PWM 参数 ───
// 50Hz 舵机 PWM: 650-1900μs, 中位 1275μs (ZTW Seal G2 非标)
// MotorCmd {throttle, steering} → STM32 坦克混控 → 左/右独立 PWM
constexpr uint16_t PWM_NEUTRAL         = 1275;  // 与 STM32 C06B 一致 (ZTW Seal G2 中位)
constexpr uint16_t PWM_MIN             = 650;
constexpr uint16_t PWM_MAX             = 1900;
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
// throttle/steering 发送到 STM32 后由坦克混控转换为左/右电机 PWM
struct MotorCmd {
    uint16_t throttle;  // 油门 (650-1900μs, 1275=停止)
    uint16_t steering;  // 转向 (650-1900μs, 1275=直行)
};
