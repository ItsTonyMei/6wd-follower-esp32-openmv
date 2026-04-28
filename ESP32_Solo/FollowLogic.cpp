#include "FollowLogic.h"
#include <cstring>

// Frame center for 192-wide window
static constexpr int FRAME_CENTER = 96;

// Compute offset from frame center (reduces duplication in update())
static int computeOffset(int cx) {
    return cx - FRAME_CENTER;
}

// Execute turn command based on offset and speed threshold
static const char* turnCommand(int offset, float threshold, const char* stopCmd, const char* turnCmd) {
    return (offset < -FollowLogic::CX_MARGIN || offset > FollowLogic::CX_MARGIN)
        ? ((offset < -FollowLogic::CX_MARGIN) ? (threshold >= FollowLogic::DIST_SCORE_SLOW ? stopCmd : "LFT:120") : (threshold >= FollowLogic::DIST_SCORE_SLOW ? stopCmd : "RGT:120"))
        : nullptr;
}

const char* FollowLogic::update(bool hasPerson, int cx, int feetY, float distScore) {
    if (!hasPerson) {
        lastCmd_ = "STOP";
        return lastCmd_;
    }

    if (distScore > 0.0f) {
        return handleDistScore(cx, distScore);
    } else {
        return handleFeetY(cx, feetY);
    }
}

const char* FollowLogic::handleDistScore(int cx, float distScore) {
    if (distScore >= DIST_SCORE_STOP) {
        return "STOP";
    }

    if (distScore < DIST_SCORE_FAR) {
        return "FWD:150";
    }

    int offset = computeOffset(cx);

    if (offset < -CX_MARGIN) {
        return (distScore >= DIST_SCORE_SLOW) ? "STOP" : "LFT:120";
    }
    if (offset > CX_MARGIN) {
        return (distScore >= DIST_SCORE_SLOW) ? "STOP" : "RGT:120";
    }

    // Centered
    if (distScore >= DIST_SCORE_SLOW && distScore < DIST_SCORE_STOP) {
        return "FWD:50";
    }
    if (distScore >= 0.50f) {
        return "FWD:100";
    }
    return "FWD:100";
}

const char* FollowLogic::handleFeetY(int cx, int feetY) {
    if (feetY >= FEETY_STOP) {
        return "STOP";
    }

    if (feetY < FEETY_FAR) {
        return "FWD:150";
    }

    int offset = computeOffset(cx);

    if (offset < -CX_MARGIN) {
        return (feetY >= FEETY_SLOW) ? "STOP" : "LFT:120";
    }
    if (offset > CX_MARGIN) {
        return (feetY >= FEETY_SLOW) ? "STOP" : "RGT:120";
    }

    return (feetY >= FEETY_SLOW) ? "STOP" : "FWD:100";
}