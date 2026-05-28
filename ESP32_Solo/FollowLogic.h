#pragma once

#include <Arduino.h>
#include "Config.h"   // MotorCmd, PWM_NEUTRAL, etc.

// ============================================================================
// FollowLogic: 人员跟随决策逻辑
// FollowLogic: person-following decision logic
//
// 根据 OpenMV 传来的视觉数据 (cx, feetY, distScore)，输出 MotorCmd 通过
// FreeRTOS queue 发送给 MotorTask。
// HC6060A 混控款电调: MotorCmd.throttle/steering 直接对应白线/黄线 PWM 脉宽 (μs)
//
// 坐标系统: 192×192 YOLO model 窗口, 中心 cx=96
// ============================================================================
class FollowLogic {
public:
    // 核心决策: 输入视觉数据 → 输出 MotorCmd (throttle μs, steering μs)
    // distScore > 0: 使用多特征融合模式; distScore == 0: 降级为 feetY 模式
    MotorCmd update(bool hasPerson, int cx, int feetY, float distScore);

    // ─── distScore 阈值 (多特征融合, 0.0-1.0, 越大越近) ───
    static constexpr float DIST_SCORE_STOP = 0.85f;  // 非常近 → STOP
    static constexpr float DIST_SCORE_SLOW = 0.65f;  // 比较近 → 慢速过渡
    static constexpr float DIST_SCORE_FAR  = 0.30f;  // 比较远 → 可全速

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
