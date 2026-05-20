// ============================================================================
// FollowLogic 单元测试
// 测试: distScore 跟随 / feetY fallback / 转向决策 / 速度选择
// ============================================================================

#include <cstdio>
#include "Config.h"
#include "FollowLogic.h"

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

static bool cmdEq(MotorCmd a, CarCmd cmd, uint8_t pwm) {
    return a.cmd == cmd && a.pwm == pwm;
}

// 无人 → STOP
void test_no_person() {
    printf("[TEST] No person -> STOP\n");
    FollowLogic fl;
    MotorCmd result = fl.update(false, 96, 100, 0.0f);
    TEST_ASSERT(cmdEq(result, CarCmd::STOP, 0), "No person returns STOP");
}

// distScore >= STOP threshold → STOP
void test_dist_stop() {
    printf("[TEST] distScore >= STOP -> STOP\n");
    FollowLogic fl;
    MotorCmd result = fl.update(true, 96, 100, 0.90f);
    TEST_ASSERT(cmdEq(result, CarCmd::STOP, 0), "High distScore returns STOP");
}

// distScore 很低 → 快速前进
void test_dist_far() {
    printf("[TEST] Low distScore -> FWD:FAST_SPEED\n");
    FollowLogic fl;
    MotorCmd result = fl.update(true, 96, 50, 0.20f);
    TEST_ASSERT(cmdEq(result, CarCmd::FWD, FollowLogic::FAST_SPEED), "Far person returns FWD fast");
}

// 偏左 → 左转
void test_turn_left() {
    printf("[TEST] Person left of center -> LFT:TURN_SPEED\n");
    FollowLogic fl;
    // offset = 60 - 96 = -36, |offset| > CX_MARGIN(25)
    MotorCmd result = fl.update(true, 60, 100, 0.50f);
    TEST_ASSERT(cmdEq(result, CarCmd::LFT, FollowLogic::TURN_SPEED), "Person left returns LFT");
}

// 偏右 → 右转
void test_turn_right() {
    printf("[TEST] Person right of center -> RGT:TURN_SPEED\n");
    FollowLogic fl;
    MotorCmd result = fl.update(true, 130, 100, 0.50f);
    TEST_ASSERT(cmdEq(result, CarCmd::RGT, FollowLogic::TURN_SPEED), "Person right returns RGT");
}

// 居中 + 中等距离 → 中速前进
void test_center_medium() {
    printf("[TEST] Centered medium dist -> FWD:MEDIUM_SPEED\n");
    FollowLogic fl;
    MotorCmd result = fl.update(true, 100, 100, 0.50f);
    TEST_ASSERT(cmdEq(result, CarCmd::FWD, FollowLogic::MEDIUM_SPEED), "Centered returns FWD medium");
}

// 居中 + 较近距离 → 慢速前进
void test_center_slow() {
    printf("[TEST] Centered slow dist -> FWD:SLOW_SPEED\n");
    FollowLogic fl;
    // distScore between SLOW and STOP
    MotorCmd result = fl.update(true, 100, 100, 0.70f);
    TEST_ASSERT(cmdEq(result, CarCmd::FWD, FollowLogic::SLOW_SPEED), "Centered slow returns FWD slow");
}

// feetY fallback: 太近 → STOP
void test_feety_stop() {
    printf("[TEST] feetY fallback: close -> STOP\n");
    FollowLogic fl;
    MotorCmd result = fl.update(true, 96, 165, 0.0f);
    TEST_ASSERT(cmdEq(result, CarCmd::STOP, 0), "feetY close returns STOP");
}

// feetY fallback: 太远 → 快速前进
void test_feety_far() {
    printf("[TEST] feetY fallback: far -> FWD:FAST_SPEED\n");
    FollowLogic fl;
    MotorCmd result = fl.update(true, 96, 60, 0.0f);
    TEST_ASSERT(cmdEq(result, CarCmd::FWD, FollowLogic::FAST_SPEED), "feetY far returns FWD fast");
}

// feetY fallback: 居中 + 安全距离 → 中速前进
void test_feety_center() {
    printf("[TEST] feetY fallback: centered safe -> FWD:MEDIUM_SPEED\n");
    FollowLogic fl;
    MotorCmd result = fl.update(true, 100, 100, 0.0f);
    TEST_ASSERT(cmdEq(result, CarCmd::FWD, FollowLogic::MEDIUM_SPEED), "feetY centered returns FWD medium");
}

// feetY fallback: 偏左 + 较近 → STOP (禁转向)
void test_feety_left_close() {
    printf("[TEST] feetY fallback: left + close -> STOP\n");
    FollowLogic fl;
    // offset = 60 - 96 = -36, feetY=150 >= SLOW(145)
    MotorCmd result = fl.update(true, 60, 150, 0.0f);
    TEST_ASSERT(cmdEq(result, CarCmd::STOP, 0), "feetY left+close returns STOP");
}

int main() {
    printf("=== FollowLogic Tests ===\n\n");
    test_no_person();
    test_dist_stop();
    test_dist_far();
    test_turn_left();
    test_turn_right();
    test_center_medium();
    test_center_slow();
    test_feety_stop();
    test_feety_far();
    test_feety_center();
    test_feety_left_close();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
