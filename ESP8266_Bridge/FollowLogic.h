#pragma once
#include <Arduino.h>
#include "Config.h"

class FollowLogic {
public:
    MotorCmd update(bool hasPerson, int cx, int feetY, float distScore);

    static constexpr float DIST_SCORE_STOP = 0.85f;
    static constexpr float DIST_SCORE_SLOW = 0.65f;
    static constexpr float DIST_SCORE_FAR  = 0.30f;

    static constexpr int FEETY_STOP = 160;
    static constexpr int FEETY_SLOW = 145;
    static constexpr int FEETY_FAR  = 80;

private:
    MotorCmd handleDistScore(int cx, float distScore);
    MotorCmd handleFeetY(int cx, int feetY);
    static uint16_t throttleFromScore(float score);
    static uint16_t steeringFromOffset(int offset);
};
