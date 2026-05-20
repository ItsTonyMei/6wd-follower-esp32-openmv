#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ============================================================================
// FreeRTOS Task 函数声明（实现在 Tasks.cpp）
// FreeRTOS Task function declarations (implemented in Tasks.cpp)
// ============================================================================

// VisionTask: OpenMV UART1 解析 + FollowLogic 决策 → MotorCmd queue (Core 0, pri 3)
void visionTaskFunc(void* param);

// WebTask: WiFi AP + HTTP Dashboard (Core 1, pri 1)
void webTaskFunc(void* param);
