#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Forward declarations
class DataAggregator;
class DashboardServer;
class VisionBridge;
class FollowLogic;
class MotorTask;

void visionTaskFunc(void* param);
void webTaskFunc(void* param);