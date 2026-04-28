#pragma once
#include <Arduino.h>

// ============================================================================
// 6轮车-UniBoard: ESP32 单板控制方案统一配置
// ============================================================================

// ─── 电机引脚（ESP32 LEDC PWM → RZ7886）───
constexpr uint8_t PIN_LEFT_FORWARD  = 25;   // → RZ7886 A-1 (左电机前进)
constexpr uint8_t PIN_LEFT_REVERSE  = 26;   // → RZ7886 A-2 (左电机后退)
constexpr uint8_t PIN_RIGHT_FORWARD = 27;   // → RZ7886 B-1 (右电机前进)
constexpr uint8_t PIN_RIGHT_REVERSE = 19;   // → RZ7886 B-2 (右电机后退)

// ─── LEDC PWM 参数 ───
constexpr uint32_t PWM_FREQ_HZ       = 1000;
constexpr uint8_t  PWM_RESOLUTION     = 8;   // 8-bit: 0-255
constexpr uint8_t  PWM_RANGE          = 255;

// ─── 超声波引脚 ───
constexpr uint8_t PIN_US_LEFT_TRIG   = 32;   // → 左 HC-SR04 TRIG
constexpr uint8_t PIN_US_LEFT_ECHO   = 34;   // ← 左 HC-SR04 ECHO (仅输入引脚, 经分压)
constexpr uint8_t PIN_US_RIGHT_TRIG  = 33;   // → 右 HC-SR04 TRIG
constexpr uint8_t PIN_US_RIGHT_ECHO  = 35;   // ← 右 HC-SR04 ECHO (仅输入引脚, 经分压)

// ─── 超声波参数 ───
constexpr int          ULTRASONIC_MAX_CM      = 400;
constexpr unsigned long ULTRASONIC_INTERVAL_MS = 50;
constexpr unsigned long ULTRASONIC_TIMEOUT_US  = 25000;

// ─── 避障安全阈值 ───
// Danger zone: < OBSTACLE_MIN_CM  → immediate STOP
constexpr int    OBSTACLE_MIN_CM         = 20;
// Warning zone: < OBSTACLE_WARN_CM → redirect (turn away from blocked side)
constexpr int    OBSTACLE_WARN_CM         = 40;
// Avoidance maneuver speed
constexpr uint8_t AVOIDANCE_SPEED        = 90;
// Max avoidance duration before switching to reverse
constexpr unsigned long AVOIDANCE_TIMEOUT_MS = 2000;

// ─── Ultrasonic obstacle side flags ───
constexpr uint8_t US_SIDE_NONE  = 0;
constexpr uint8_t US_SIDE_LEFT  = 1;
constexpr uint8_t US_SIDE_RIGHT = 2;
constexpr uint8_t US_SIDE_BOTH  = 3;

// ─── Command timeout (motor stops if no command received within this period) ───
constexpr unsigned long COMMAND_TIMEOUT_MS = 500;

// ─── WiFi AP 配置 ───
// WiFi AP 配置（敏感信息，公开仓库前请替换为实际值，或改为环境变量读取）
constexpr char WIFI_SSID[] = "6轮车仪表盘";
constexpr char WIFI_PASS[] = "YOUR_WIFI_PASSWORD";

// ─── FreeRTOS 任务栈大小 ───
constexpr uint32_t TASK_STACK_SIZE = 4096;

// ─── 视觉数据超时 (ms) ───
constexpr unsigned long VISION_TIMEOUT_MS = 700;

// ─── 电机死区时间 (ms) ───
constexpr uint32_t MOTOR_DEAD_TIME_MS = 5;

// ─── Motor command type (zero-alloc queue element, 2 bytes) ───
enum class CarCmd : uint8_t { STOP, FWD, REV, LFT, RGT };

struct MotorCmd {
    CarCmd   cmd;
    uint8_t  pwm;    // 0-255
};
