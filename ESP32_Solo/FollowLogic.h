#pragma once

#include <Arduino.h>
#include "Config.h"   // MotorCmd, CarCmd

// ============================================================================
// FollowLogic: 人员跟随决策逻辑
// FollowLogic: person-following decision logic
//
// 根据 OpenMV 传来的视觉数据 (cx, feetY, distScore)，输出 MotorCmd 通过
// FreeRTOS queue 发送给 MotorTask。支持两种模式：
//   1. distScore 模式 (primary): 多特征融合距离分 (0.0-1.0, 越大越近)
//   2. feetY fallback 模式 (legacy): distScore==0 时降级为 feetY 判断
//
// 坐标系统: 192×192 YOLO model 窗口, 中心 cx=96
// ============================================================================
class FollowLogic {
public:
    // 核心决策: 输入视觉数据 → 输出 MotorCmd
    // distScore > 0: 使用多特征融合模式; distScore == 0: 降级为 feetY 模式
    MotorCmd update(bool hasPerson, int cx, int feetY, float distScore);

    // ─── PWM 速度常量 (0-255) ───
    static constexpr uint8_t FAST_SPEED   = 150;  // 远距离快速接近
    static constexpr uint8_t MEDIUM_SPEED = 100;  // 中等距离
    static constexpr uint8_t TURN_SPEED   = 120;  // 转向速度
    static constexpr uint8_t SLOW_SPEED   = 50;   // 近距离慢速

    // ─── feetY 阈值 (legacy fallback, 192×192 窗口) ───
    static constexpr int FEETY_STOP  = 160;  // 太近 → STOP
    static constexpr int FEETY_SLOW  = 145;  // 比较近 → 慢速
    static constexpr int FEETY_FAR   = 80;   // 比较远 → 加速

    // ─── cx 横向偏移阈值 (192-wide 窗口, center=96) ───
    static constexpr int CX_MARGIN   = 25;   // 中心区宽度 ±25px

    // ─── distScore 阈值 (多特征融合, 0.0-1.0, 越大越近) ───
    static constexpr float DIST_SCORE_STOP = 0.85f;  // 非常近 → STOP
    static constexpr float DIST_SCORE_SLOW = 0.65f;  // 比较近 → 慢速过渡
    static constexpr float DIST_SCORE_FAR  = 0.30f;  // 比较远 → 可全速

private:
    MotorCmd handleDistScore(int cx, float distScore);
    MotorCmd handleFeetY(int cx, int feetY);
};
