#include "FollowLogic.h"

static constexpr int FRAME_CENTER = 96;

MotorCmd FollowLogic::update(bool hasPerson, int cx, int feetY, float distScore) {
    if (!hasPerson) {
        return {PWM_NEUTRAL, PWM_NEUTRAL};
    }
    if (distScore > 0.0f) {
        return handleDistScore(cx, distScore);
    } else {
        return handleFeetY(cx, feetY);
    }
}

uint16_t FollowLogic::throttleFromScore(float score) {
    // ─── 双向油门: score=0.5=停止, <0.5=前进, >0.5=后退 ───
    // 强制停止: 极限逼近 (>0.9)
    if (score >= SCORE_HARD_STOP) {
        return PWM_NEUTRAL;
    }

    float centered = SCORE_NEUTRAL - score;  // 远(+)=前进, 近(-)=后退
    float range = 0.5f - SCORE_DEADBAND;     // 有效范围 (0.46)

    if (abs(centered) <= SCORE_DEADBAND) {
        return PWM_NEUTRAL;                   // 死区 → 停止
    }

    float scale = (abs(centered) - SCORE_DEADBAND) / range;
    if (scale > 1.0f) scale = 1.0f;

    int offset = (int)(scale * MAX_THROTTLE_OFFSET);
    if (centered > 0.0f) {
        return PWM_NEUTRAL + offset;          // 前进
    } else {
        return PWM_NEUTRAL - offset;          // 后退
    }
}

uint16_t FollowLogic::steeringFromOffset(int offset) {
    if (abs(offset) <= CX_MIN_OFFSET) {
        return PWM_NEUTRAL;
    }
    int steer = (int)PWM_NEUTRAL + (int)((float)offset * TURN_KP);
    if (steer > (int)(PWM_NEUTRAL + MAX_STEER_OFFSET))
        steer = PWM_NEUTRAL + MAX_STEER_OFFSET;
    if (steer < (int)(PWM_NEUTRAL - MAX_STEER_OFFSET))
        steer = PWM_NEUTRAL - MAX_STEER_OFFSET;
    return (uint16_t)steer;
}

MotorCmd FollowLogic::handleDistScore(int cx, float distScore) {
    uint16_t throttle = throttleFromScore(distScore);
    int offset = cx - FRAME_CENTER;
    uint16_t steering = steeringFromOffset(offset);

    // 距离很近时锁转向，防止擦撞
    if (distScore >= SCORE_STEER_LOCK) {
        steering = PWM_NEUTRAL;
    }

    return {throttle, steering};
}

MotorCmd FollowLogic::handleFeetY(int cx, int feetY) {
    float pseudoScore;

    if (feetY >= FEETY_STOP) {
        pseudoScore = SCORE_HARD_STOP;               // 太近 → 0.90
    } else if (feetY >= FEETY_SLOW) {
        // feetY: 145→160 → score: 0.70→0.90
        pseudoScore = SCORE_STEER_LOCK
            + (SCORE_HARD_STOP - SCORE_STEER_LOCK)
            * (float)(feetY - FEETY_SLOW) / (float)(FEETY_STOP - FEETY_SLOW);
    } else if (feetY >= FEETY_FAR) {
        // feetY: 80→145 → score: 0.30→0.70
        pseudoScore = 0.30f
            + (SCORE_STEER_LOCK - 0.30f)
            * (float)(feetY - FEETY_FAR) / (float)(FEETY_SLOW - FEETY_FAR);
    } else {
        pseudoScore = 0.25f;
    }

    return handleDistScore(cx, pseudoScore);
}
