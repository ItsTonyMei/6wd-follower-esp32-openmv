#pragma once
#include <Arduino.h>

// ============================================================================
// 6WD Follower — ESP32 UniBoard 统一配置
// 所有引脚、阈值、任务参数的单一起源 (single source of truth)
// ============================================================================

// ─── 电机引脚 (ESP32 LEDC PWM → RZ7886 H-bridge) ───
constexpr uint8_t PIN_LEFT_FORWARD  = 25;   // → RZ7886 A-1 (左电机前进)
constexpr uint8_t PIN_LEFT_REVERSE  = 26;   // → RZ7886 A-2 (左电机后退)
constexpr uint8_t PIN_RIGHT_FORWARD = 27;   // → RZ7886 B-1 (右电机前进)
constexpr uint8_t PIN_RIGHT_REVERSE = 19;   // → RZ7886 B-2 (右电机后退)

// ─── LEDC PWM 参数 ───
constexpr uint32_t PWM_FREQ_HZ       = 1000;
constexpr uint8_t  PWM_RESOLUTION     = 8;    // 8-bit: 0-255
constexpr uint8_t  PWM_RANGE          = 255;

// ─── 超声波引脚 (HC-SR04 ×2) ───
constexpr uint8_t PIN_US_LEFT_TRIG   = 32;   // → 左 HC-SR04 TRIG
constexpr uint8_t PIN_US_LEFT_ECHO   = 34;   // ← 左 HC-SR04 ECHO (仅输入, 经分压至 3.3V)
constexpr uint8_t PIN_US_RIGHT_TRIG  = 33;   // → 右 HC-SR04 TRIG
constexpr uint8_t PIN_US_RIGHT_ECHO  = 35;   // ← 右 HC-SR04 ECHO (仅输入, 经分压至 3.3V)

// ─── 超声波参数 ───
constexpr int           ULTRASONIC_MAX_CM       = 400;
constexpr unsigned long ULTRASONIC_INTERVAL_MS  = 50;
constexpr unsigned long ULTRASONIC_TIMEOUT_US   = 25000;

// ─── 避障安全阈值 ───
// 危险区 (Danger zone):  < OBSTACLE_MIN_CM → 强制 STOP
constexpr int    OBSTACLE_MIN_CM         = 20;
// 警告区 (Warning zone): < OBSTACLE_WARN_CM → 避障转向
constexpr int    OBSTACLE_WARN_CM         = 40;
// 避障转向速度
constexpr uint8_t AVOIDANCE_SPEED        = 90;
// 避障超时后切换为后退策略
constexpr unsigned long AVOIDANCE_TIMEOUT_MS = 2000;

// ─── 超声波障碍方向 flags ───
constexpr uint8_t US_SIDE_NONE  = 0;
constexpr uint8_t US_SIDE_LEFT  = 1;
constexpr uint8_t US_SIDE_RIGHT = 2;
constexpr uint8_t US_SIDE_BOTH  = 3;

// ─── 命令超时 (ms): 超时无命令 → STOP ───
constexpr unsigned long COMMAND_TIMEOUT_MS = 500;

// ─── WiFi AP 配置 ───
constexpr char WIFI_SSID[] = "6WD Dashboard";
constexpr char WIFI_PASS[] = "YOUR_WIFI_PASSWORD";

// ─── FreeRTOS task stack size ───
constexpr uint32_t TASK_STACK_SIZE = 4096;

// ─── 视觉数据超时 (ms): 超过此时间无 VIS 帧 → 视为失效 ───
constexpr unsigned long VISION_TIMEOUT_MS = 700;

// ─── 电机死区时间 (ms): H-bridge 方向切换时短暂停顿防直通 ───
constexpr uint32_t MOTOR_DEAD_TIME_MS = 5;

// ─── MotorCmd: FreeRTOS queue 元素 (2 字节, 零 heap 分配) ───
enum class CarCmd : uint8_t { STOP, FWD, REV, LFT, RGT };

struct MotorCmd {
    CarCmd   cmd;
    uint8_t  pwm;    // 0-255
};
