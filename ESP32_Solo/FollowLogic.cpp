#include "FollowLogic.h"

// 192-wide model 窗口中心
static constexpr int FRAME_CENTER = 96;

MotorCmd FollowLogic::update(bool hasPerson, int cx, int feetY, float distScore) {
    if (!hasPerson) {
        return {CarCmd::STOP, 0};
    }

    if (distScore > 0.0f) {
        return handleDistScore(cx, distScore);
    } else {
        return handleFeetY(cx, feetY);
    }
}

MotorCmd FollowLogic::handleDistScore(int cx, float distScore) {
    // 非常近 → STOP
    if (distScore >= DIST_SCORE_STOP) {
        return {CarCmd::STOP, 0};
    }

    // 比较远 → 快速接近
    if (distScore < DIST_SCORE_FAR) {
        return {CarCmd::FWD, FAST_SPEED};
    }

    int offset = cx - FRAME_CENTER;

    // 偏左 → 左转（近距离禁转向，防止撞到人）
    if (offset < -CX_MARGIN) {
        return (distScore >= DIST_SCORE_SLOW)
            ? MotorCmd{CarCmd::STOP, 0}
            : MotorCmd{CarCmd::LFT, TURN_SPEED};
    }
    // 偏右 → 右转
    if (offset > CX_MARGIN) {
        return (distScore >= DIST_SCORE_SLOW)
            ? MotorCmd{CarCmd::STOP, 0}
            : MotorCmd{CarCmd::RGT, TURN_SPEED};
    }

    // 居中 → 根据距离选择速度
    if (distScore >= DIST_SCORE_SLOW && distScore < DIST_SCORE_STOP) {
        return {CarCmd::FWD, SLOW_SPEED};       // 较近 → 慢速
    }
    if (distScore >= 0.50f) {
        return {CarCmd::FWD, MEDIUM_SPEED};     // 中等距离
    }
    return {CarCmd::FWD, MEDIUM_SPEED};
}

MotorCmd FollowLogic::handleFeetY(int cx, int feetY) {
    // 太近 → STOP
    if (feetY >= FEETY_STOP) {
        return {CarCmd::STOP, 0};
    }

    // 比较远 → 快速接近
    if (feetY < FEETY_FAR) {
        return {CarCmd::FWD, FAST_SPEED};
    }

    int offset = cx - FRAME_CENTER;

    // 偏左 → 左转（近距离禁转向）
    if (offset < -CX_MARGIN) {
        return (feetY >= FEETY_SLOW)
            ? MotorCmd{CarCmd::STOP, 0}
            : MotorCmd{CarCmd::LFT, TURN_SPEED};
    }
    // 偏右 → 右转
    if (offset > CX_MARGIN) {
        return (feetY >= FEETY_SLOW)
            ? MotorCmd{CarCmd::STOP, 0}
            : MotorCmd{CarCmd::RGT, TURN_SPEED};
    }

    // 居中 → 安全距离前进 / 较近时 STOP
    return (feetY >= FEETY_SLOW)
        ? MotorCmd{CarCmd::STOP, 0}
        : MotorCmd{CarCmd::FWD, MEDIUM_SPEED};
}
