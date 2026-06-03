#pragma once
#include <Arduino.h>

// ============================================================================
// 履带车视觉跟随系统 — ESP32 精简配置 (2026-06-03 重新启用)
// 功能与 ESP8266 一致: VIS接收 + FollowLogic + STM32通信 + WiFi Dashboard
// ============================================================================
//
// 架构: OpenMV (L1感知) → ESP32 (L2决策+WiFi) → STM32 (L3执行+安全)
// 供电: 48V 89Ah 锂电池 → 48V→5V 10A 防水降压 → ESP32 (USB)
// 动力: 两台三相无刷电机 + 双路独立无刷 ESC (ZTW Seal G2, 中位=1275μs)
// STM32 端坦克混控: left = thr + (st-PWM_NEUTRAL), right = thr - (st-PWM_NEUTRAL)

// ─── 引脚分配 ───
// ┌──────────────────┬────────────────┬─────────────────────────────┐
// │  功能             │ 引脚            │ 目标                         │
// ├──────────────────┼────────────────┼─────────────────────────────┤
// │ Debug (UART0)    │ GPIO1/3        │ USB-Serial (CH340/CP2102)   │
// │ VIS RX (SW Ser)  │ GPIO4          │ ← OpenMV P2 UART4           │
// │ STM32 TX (UART2) │ GPIO17         │ → STM32 PB11 (USART3 RX)    │
// │ STM32 RX (UART2) │ GPIO16         │ ← STM32 PB10 (USART3 TX)    │
// └──────────────────┴────────────────┴─────────────────────────────┘

// ─── 板载 LED ───
constexpr uint8_t PIN_LED       = 2;    // GPIO2 (ESP32 DevKit V1 板载蓝色 LED, active-HIGH, 已验证 2026-06-03)
// 行为: 常亮=运行中等待VIS, 闪烁=VIS帧接收中

// ─── VIS 接收 (SoftwareSerial) ───
constexpr uint8_t PIN_VIS_RX    = 4;    // ← OpenMV P2 UART4 TX @ 4800 baud

// ─── STM32 通信 (UART2 / Serial2) ───
constexpr uint8_t PIN_STM32_TX  = 17;   // → STM32 PB11
constexpr uint8_t PIN_STM32_RX  = 16;   // ← STM32 PB10
constexpr uint32_t STM32_BAUD   = 115200;

// ─── WiFi AP ───
constexpr char WIFI_SSID[] = "Tracked Robot";
constexpr char WIFI_PASS[] = "12345678";

// ─── 双路无刷电调 PWM 参数 (与 STM32 一致) ───
// ZTW Seal G2 电调中位=1275μs (非标), PWM 范围 650-1900μs
// MotorCmd {throttle, steering} → STM32 坦克混控 → 左/右独立 PWM
// ESP32 自动模式限幅 = PS2 遥控器的 50% (安全考虑)
constexpr uint16_t PWM_NEUTRAL         = 1275;  // 中位=停止 (与 STM32 一致)
constexpr uint16_t PWM_MIN             = 650;   // 下限
constexpr uint16_t PWM_MAX             = 1900;  // 上限
constexpr uint16_t MAX_THROTTLE_OFFSET = 300;   // 最大油门偏移 (PS2=600, 自动=50%)
constexpr uint16_t MAX_STEER_OFFSET    = 200;   // 最大转向偏移 (PS2=400, 自动=50%)
constexpr uint16_t THROTTLE_DEADBAND   = 20;
constexpr float    TURN_KP             = 2.0f;
constexpr int      CX_MIN_OFFSET       = 15;

// ─── 时序参数 ───
constexpr uint32_t STM32_CMD_INTERVAL_MS = 50;
constexpr uint32_t CMD_TIMEOUT_MS        = 500;
constexpr uint32_t VISION_TIMEOUT_MS     = 700;
constexpr uint32_t ESC_INIT_DELAY_MS     = 3000;

// ─── 系统参数 ───
constexpr uint32_t DEBUG_BAUD       = 115200;  // USB 串口波特率
constexpr uint32_t VIS_BAUD         = 4800;    // OpenMV→ESP32 VIS 波特率

// ─── VIS 接收 ───
constexpr uint16_t VIS_BUF_SIZE     = 256;     // VIS 行缓冲区 (字节)
constexpr uint32_t DIAG_INTERVAL_MS = 5000;    // VIS 诊断输出间隔 (0=关闭)

// ─── MotorCmd ───
// throttle/steering 发送到 STM32 后由坦克混控转换为左/右电机 PWM
// 范围 650-1900μs, 中位 1275μs (与 STM32 C06B 一致)
struct MotorCmd {
    uint16_t throttle;  // 油门 (650-1900μs, 1275=停止, 自动限幅±300)
    uint16_t steering;  // 转向 (650-1900μs, 1275=直行, 自动限幅±200)
};
