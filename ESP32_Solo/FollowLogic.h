#pragma once

#include <Arduino.h>
#include "Config.h"   // MotorCmd, PWM_NEUTRAL, etc.

// ============================================================================
// FollowLogic: 人员跟随决策逻辑
// FollowLogic: person-following decision logic
//
// 根据 OpenMV 传来的视觉数据 (cx, feetY, distScore)，输出 MotorCmd
// {throttle, steering} 通过串口发送到 STM32 做坦克混控。
// 坐标系统: 192×192 YOLO model 窗口, 中心 cx=96
// ESP32 与 ESP8266 共享此算法 (完全相同)。
// ============================================================================
class FollowLogic {
public:
    // 核心决策: 输入视觉数据 → 输出 MotorCmd (throttle μs, steering μs)
    // distScore > 0: 使用多特征融合模式; distScore == 0: 降级为 feetY 模式
    MotorCmd update(bool hasPerson, int cx, int feetY, float distScore);

    // ─── distScore 阈值 (双向: 0.0=远/前进, 0.5=1.5m/停止, 1.0=近/后退) ───
    static constexpr float SCORE_NEUTRAL     = 0.50f;  // 目标距离 (1.5m), 停止
    static constexpr float SCORE_DEADBAND    = 0.04f;  // 停止死区 (±2% ≈ ±6cm @1.5m)
    static constexpr float SCORE_STEER_LOCK  = 0.70f;  // > this → 锁转向 (太近防撞)
    static constexpr float SCORE_HARD_STOP   = 0.90f;  // > this → 强制停止 (极限逼近)

    // ─── feetY 阈值 (legacy fallback, 192×192 窗口) ───
    static constexpr int FEETY_STOP  = 160;  // 太近 → STOP
    static constexpr int FEETY_SLOW  = 145;  // 比较近 → 慢速
    static constexpr int FEETY_FAR   = 80;   // 比较远 → 加速

private:
    MotorCmd handleDistScore(int cx, float distScore);
    MotorCmd handleFeetY(int cx, int feetY);

    // 根据距离分计算油门脉宽 (1500±offset μs)
    static uint16_t throttleFromScore(float score);
    // 根据横向偏移量计算转向脉宽 (1500±offset μs)
    static uint16_t steeringFromOffset(int offset);
};
