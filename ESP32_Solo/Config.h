#pragma once
#include <Arduino.h>

// ============================================================================
// 履带车视觉跟随系统 — ESP32 精简配置 (2026-06-03 重新启用)
// 功能与 ESP8266 一致: VIS接收 + FollowLogic + STM32通信 + WiFi Dashboard
// ============================================================================
//
// 架构: OpenMV (L1感知) → ESP32 (L2决策+WiFi) → STM32 (L3执行+安全)
// 供电: 48V 89Ah 锂电池 → 48V→5V 10A 防水降压 → ESP32 (USB)
// 动力: 两台三相无刷电机 + 双路独立无刷 ESC
// STM32 端坦克混控: left = thr + (st-1500), right = thr - (st-1500)

// ─── 引脚分配 ───
// ┌──────────────────┬────────────────┬─────────────────────────────┐
// │  功能             │ 引脚            │ 目标                         │
// ├──────────────────┼────────────────┼─────────────────────────────┤
// │ Debug (UART0)    │ GPIO1/3        │ USB-Serial (CH340/CP2102)   │
// │ VIS RX (SW Ser)  │ GPIO4          │ ← OpenMV P0 HW UART(3)      │
// │ STM32 TX (UART2) │ GPIO17         │ → STM32 PB11 (USART3 RX)    │
// │ STM32 RX (UART2) │ GPIO16         │ ← STM32 PB10 (USART3 TX)    │
// └──────────────────┴────────────────┴─────────────────────────────┘

// ─── 板载 LED ───
constexpr uint8_t PIN_LED       = 2;    // GPIO2 (ESP32 DevKit V1 板载蓝色 LED, active-HIGH, 已验证 2026-06-03)
// 行为: 常亮=运行中等待VIS, 闪烁=VIS帧接收中

// ─── VIS 接收 (SoftwareSerial) ───
constexpr uint8_t PIN_VIS_RX    = 4;    // ← OpenMV P0 HW UART(3) @ 4800 baud

// ─── STM32 通信 (UART2 / Serial2) ───
constexpr uint8_t PIN_STM32_TX  = 17;   // → STM32 PB11
constexpr uint8_t PIN_STM32_RX  = 16;   // ← STM32 PB10
constexpr uint32_t STM32_BAUD   = 115200;

// ─── WiFi AP ───
constexpr char WIFI_SSID[] = "Tracked Robot";
constexpr char WIFI_PASS[] = "12345678";

// ─── 双路无刷电调 PWM 参数 ───
// 50Hz 舵机 PWM: 1000-2000μs, 中位 1500μs
// MotorCmd {throttle, steering} → STM32 坦克混控 → 左/右独立 PWM
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
// throttle/steering 发送到 STM32 后由坦克混控转换为左/右电机 PWM
struct MotorCmd {
    uint16_t throttle;  // 油门 (1000-2000μs, 1500=停止)
    uint16_t steering;  // 转向 (1000-2000μs, 1500=直行)
};
