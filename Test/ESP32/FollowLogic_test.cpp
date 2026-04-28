// ============================================================================
// FollowLogic Unit Tests
// Tests: distScore-based following, feetY fallback, turn decisions
// ============================================================================

#include <cstdio>
#include <cstring>
#include "FollowLogic.h"

// Test helper
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (cond) { \
            printf("[PASS] %s\n", msg); \
            tests_passed++; \
        } else { \
            printf("[FAIL] %s\n", msg); \
            tests_failed++; \
        } \
    } while(0)

// Test: no person → STOP
bool test_no_person() {
    printf("[TEST] No person detected → STOP\n");
    FollowLogic fl;
    const char* result = fl.update(false, 96, 100, 0.0f);
    TEST_ASSERT(strcmp(result, "STOP") == 0, "No person returns STOP");
    return true;
}

// Test: distScore >= STOP threshold → STOP
bool test_dist_stop() {
    printf("[TEST] distScore >= STOP → STOP\n");
    FollowLogic fl;
    const char* result = fl.update(true, 96, 100, 0.90f);  // > DIST_SCORE_STOP=0.85
    TEST_ASSERT(strcmp(result, "STOP") == 0, "High distScore returns STOP");
    return true;
}

// Test: distScore very low → FWD:150 (fast forward)
bool test_dist_far() {
    printf("[TEST] Low distScore → FWD:150\n");
    FollowLogic fl;
    const char* result = fl.update(true, 96, 50, 0.20f);  // < DIST_SCORE_FAR=0.30
    TEST_ASSERT(strcmp(result, "FWD:150") == 0, "Far person returns FWD:150");
    return true;
}

// Test: person left of center → LFT:120
bool test_turn_left() {
    printf("[TEST] Person left of center → LFT:120\n");
    FollowLogic fl;
    // offset = 60 - 96 = -36, |offset| > CX_MARGIN(25), so turn
    const char* result = fl.update(true, 60, 100, 0.50f);  // medium dist, person left
    TEST_ASSERT(strcmp(result, "LFT:120") == 0, "Person left returns LFT:120");
    return true;
}

// Test: person right of center → RGT:120
bool test_turn_right() {
    printf("[TEST] Person right of center → RGT:120\n");
    FollowLogic fl;
    // offset = 130 - 96 = 34, |offset| > CX_MARGIN(25), so turn
    const char* result = fl.update(true, 130, 100, 0.50f);  // medium dist, person right
    TEST_ASSERT(strcmp(result, "RGT:120") == 0, "Person right returns RGT:120");
    return true;
}

// Test: person centered, medium dist → FWD:100
bool test_center_medium() {
    printf("[TEST] Person centered, medium dist → FWD:100\n");
    FollowLogic fl;
    const char* result = fl.update(true, 100, 100, 0.50f);  // offset=4, within margin
    TEST_ASSERT(strcmp(result, "FWD:100") == 0, "Centered returns FWD:100");
    return true;
}

// Test: feetY fallback - person close → STOP
bool test_feety_stop() {
    printf("[TEST] feetY fallback: close → STOP\n");
    FollowLogic fl;
    // distScore=0 triggers feetY fallback; feetY=165 > FEETY_STOP(160)
    const char* result = fl.update(true, 96, 165, 0.0f);
    TEST_ASSERT(strcmp(result, "STOP") == 0, "feetY close returns STOP");
    return true;
}

// Test: feetY fallback - person far → FWD:150
bool test_feety_far() {
    printf("[TEST] feetY fallback: far → FWD:150\n");
    FollowLogic fl;
    // feetY=60 < FEETY_FAR(80)
    const char* result = fl.update(true, 96, 60, 0.0f);
    TEST_ASSERT(strcmp(result, "FWD:150") == 0, "feetY far returns FWD:150");
    return true;
}

// Test: feetY fallback - centered, not close → FWD:100
bool test_feety_center() {
    printf("[TEST] feetY fallback: centered, safe → FWD:100\n");
    FollowLogic fl;
    // 80 <= feetY < 155, centered
    const char* result = fl.update(true, 100, 100, 0.0f);
    TEST_ASSERT(strcmp(result, "FWD:100") == 0, "feetY centered returns FWD:100");
    return true;
}

int main() {
    printf("=== FollowLogic Tests ===\n");
    test_no_person();
    test_dist_stop();
    test_dist_far();
    test_turn_left();
    test_turn_right();
    test_center_medium();
    test_feety_stop();
    test_feety_far();
    test_feety_center();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}