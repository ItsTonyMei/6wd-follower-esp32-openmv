#pragma once

#include <Arduino.h>

// Follow logic thresholds (tuned for 192x192 YOLO window)
// feet_y: larger = person is lower in frame = closer to camera
// cx: frame center is 96 (for 192-wide window)
// Person center x position determines turn direction
// feet_y determines forward/backward movement
// dist_score: 0.0-1.0 multi-feature fusion score (larger = closer)

class FollowLogic {
public:
    // Call with current vision state; returns car command string to send to ESP8266
    const char* update(bool hasPerson, int cx, int feetY, float distScore);

    // Follower speed (PWM 0-255)
    static constexpr int FOLLOW_SPEED = 120;
    static constexpr int TURN_SPEED   = 100;

    // feet_y thresholds (legacy, used when dist_score unavailable)
    static constexpr int FEETY_STOP   = 160;  // person too close, stop
    static constexpr int FEETY_SLOW  = 145;  // person close, slow down
    static constexpr int FEETY_FAR   = 80;   // person too far, move forward

    // cx thresholds (for 192-wide window, center=96)
    static constexpr int CX_MARGIN   = 25;   // center zone width = ±25

    // dist_score thresholds (multi-feature fusion, 0.0-1.0, larger=closer)
    static constexpr float DIST_SCORE_STOP = 0.85f;  // very close, stop
    static constexpr float DIST_SCORE_SLOW = 0.65f;  // close, slow down
    static constexpr float DIST_SCORE_FAR  = 0.30f;  // far, can move fast

private:
    const char* handleDistScore(int cx, float distScore);
    const char* handleFeetY(int cx, int feetY);
    const char* lastCmd_ = "STOP";
};