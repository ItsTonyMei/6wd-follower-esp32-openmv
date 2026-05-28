#include "FollowLogic.h"

static constexpr int FRAME_CENTER = 96;

MotorCmd FollowLogic::update(bool hasPerson, int cx, int feetY, float distScore) {
    if (!hasPerson) return {PWM_NEUTRAL, PWM_NEUTRAL};
    if (distScore > 0.0f) return handleDistScore(cx, distScore);
    return handleFeetY(cx, feetY);
}

uint16_t FollowLogic::throttleFromScore(float score) {
    if (score >= DIST_SCORE_STOP) return PWM_NEUTRAL;
    if (score < DIST_SCORE_FAR)  return PWM_NEUTRAL + MAX_THROTTLE_OFFSET;

    if (score < DIST_SCORE_SLOW) {
        float t = (score - DIST_SCORE_FAR) / (DIST_SCORE_SLOW - DIST_SCORE_FAR);
        uint16_t off = MAX_THROTTLE_OFFSET - (uint16_t)(t * (MAX_THROTTLE_OFFSET - 100));
        return PWM_NEUTRAL + off;
    }

    float t = (score - DIST_SCORE_SLOW) / (DIST_SCORE_STOP - DIST_SCORE_SLOW);
    uint16_t off = (uint16_t)(100.0f * (1.0f - t));
    return PWM_NEUTRAL + (off > THROTTLE_DEADBAND ? off : 0);
}

uint16_t FollowLogic::steeringFromOffset(int offset) {
    if (abs(offset) <= CX_MIN_OFFSET) return PWM_NEUTRAL;
    int s = (int)PWM_NEUTRAL + (int)((float)offset * TURN_KP);
    if (s > (int)(PWM_NEUTRAL + MAX_STEER_OFFSET)) s = PWM_NEUTRAL + MAX_STEER_OFFSET;
    if (s < (int)(PWM_NEUTRAL - MAX_STEER_OFFSET)) s = PWM_NEUTRAL - MAX_STEER_OFFSET;
    return (uint16_t)s;
}

MotorCmd FollowLogic::handleDistScore(int cx, float distScore) {
    uint16_t throttle = throttleFromScore(distScore);
    int offset = cx - FRAME_CENTER;
    uint16_t steering = steeringFromOffset(offset);
    if (distScore >= DIST_SCORE_SLOW) steering = PWM_NEUTRAL;
    return {throttle, steering};
}

MotorCmd FollowLogic::handleFeetY(int cx, int feetY) {
    float pseudo;
    if (feetY >= FEETY_STOP)
        pseudo = 0.90f;
    else if (feetY >= FEETY_SLOW)
        pseudo = DIST_SCORE_SLOW + (DIST_SCORE_STOP - DIST_SCORE_SLOW)
               * (float)(feetY - FEETY_SLOW) / (float)(FEETY_STOP - FEETY_SLOW);
    else if (feetY >= FEETY_FAR)
        pseudo = DIST_SCORE_FAR + (DIST_SCORE_SLOW - DIST_SCORE_FAR)
               * (float)(feetY - FEETY_FAR) / (float)(FEETY_SLOW - FEETY_FAR);
    else
        pseudo = DIST_SCORE_FAR - 0.05f;
    return handleDistScore(cx, pseudo);
}
