// ============================================================================
// FollowLogic 单元测试 (HC6060A 混控款)
// 测试: throttle/steering PWM 脉宽输出、距离映射、转向比例控制
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
            printf("[FAIL] %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            tests_failed++; \
        } \
    } while(0)

// ─── 无人 → 中位 ───
void test_no_person() {
    printf("[TEST] No person -> NEUTRAL\n");
    FollowLogic fl;
    MotorCmd r = fl.update(false, 96, 100, 0.0f);
    TEST_ASSERT(r.throttle == PWM_NEUTRAL && r.steering == PWM_NEUTRAL,
                "No person returns NEUTRAL (1500,1500)");
}

// ─── distScore >= STOP → 油门归中, 转向归中 ───
void test_dist_stop() {
    printf("[TEST] distScore >= STOP -> throttle=1500\n");
    FollowLogic fl;
    MotorCmd r = fl.update(true, 96, 100, 0.90f);
    TEST_ASSERT(r.throttle == PWM_NEUTRAL,
                "STOP threshold returns throttle=1500");
}

// ─── 远处有人, 居中 → 快速前进, 直行 ───
void test_dist_far_center() {
    printf("[TEST] Far person centered -> full throttle, straight\n");
    FollowLogic fl;
    MotorCmd r = fl.update(true, 96, 50, 0.15f);
    TEST_ASSERT(r.throttle > (PWM_NEUTRAL + 300),
                "Far person: throttle significantly above neutral");
    TEST_ASSERT(r.steering == PWM_NEUTRAL,
                "Centered person: steering stays NEUTRAL");
}

// ─── 人偏左 (cx=60) → 车左转跟随 → steering < 1500 ───
void test_person_left_steer_left() {
    printf("[TEST] Person left -> vehicle turns LEFT (<1500)\n");
    FollowLogic fl;
    MotorCmd r = fl.update(true, 60, 100, 0.40f);
    TEST_ASSERT(r.steering < PWM_NEUTRAL,
                "Person left (cx=60): steering < 1500 (vehicle turns left)");
    TEST_ASSERT(r.throttle > PWM_NEUTRAL,
                "Still moving forward");
}

// ─── 人偏右 (cx=130) → 车右转跟随 → steering > 1500 ───
void test_person_right_steer_right() {
    printf("[TEST] Person right -> vehicle turns RIGHT (>1500)\n");
    FollowLogic fl;
    MotorCmd r = fl.update(true, 130, 100, 0.40f);
    TEST_ASSERT(r.steering > PWM_NEUTRAL,
                "Person right (cx=130): steering > 1500 (vehicle turns right)");
    TEST_ASSERT(r.throttle > PWM_NEUTRAL,
                "Still moving forward");
}

// ─── 人偏移在死区内 → 转向归中 ───
void test_deadband_center() {
    printf("[TEST] Offset within deadband -> steering=1500\n");
    FollowLogic fl;
    // cx=100, offset=4, within CX_MIN_OFFSET=15
    MotorCmd r = fl.update(true, 100, 100, 0.40f);
    TEST_ASSERT(r.steering == PWM_NEUTRAL,
                "Small offset: steering = NEUTRAL");
}

// ─── 中距离 → 油门偏中 ───
void test_medium_distance() {
    printf("[TEST] Medium distance -> moderate throttle\n");
    FollowLogic fl;
    MotorCmd r = fl.update(true, 96, 100, 0.50f);
    TEST_ASSERT(r.throttle > (PWM_NEUTRAL + 100) &&
                r.throttle < (PWM_NEUTRAL + MAX_THROTTLE_OFFSET),
                "Medium distance: throttle in mid-range");
}

// ─── 近距离 → 慢速, 禁止转向 ───
void test_near_slow_no_steer() {
    printf("[TEST] Near person -> slow, steering suppressed\n");
    FollowLogic fl;
    MotorCmd r = fl.update(true, 60, 100, 0.70f);
    TEST_ASSERT(r.throttle <= (PWM_NEUTRAL + 150),
                "Near: throttle near NEUTRAL (slow)");
    TEST_ASSERT(r.steering == PWM_NEUTRAL,
                "Near: steering suppressed to avoid collision");
}

// ─── feetY fallback: 太近 → STOP ───
void test_feety_stop() {
    printf("[TEST] feetY fallback: close -> STOP\n");
    FollowLogic fl;
    MotorCmd r = fl.update(true, 96, 165, 0.0f);
    TEST_ASSERT(r.throttle == PWM_NEUTRAL,
                "feetY close returns throttle=1500");
}

// ─── feetY fallback: 远 → 快速 ───
void test_feety_far() {
    printf("[TEST] feetY fallback: far -> full speed\n");
    FollowLogic fl;
    MotorCmd r = fl.update(true, 96, 60, 0.0f);
    TEST_ASSERT(r.throttle > (PWM_NEUTRAL + 300),
                "feetY far returns high throttle");
}

// ─── feetY fallback: 偏左+近 → STOP, 不转 ───
void test_feety_left_close() {
    printf("[TEST] feetY fallback: left + close -> STOP, no steer\n");
    FollowLogic fl;
    MotorCmd r = fl.update(true, 60, 150, 0.0f);
    TEST_ASSERT(r.steering == PWM_NEUTRAL,
                "feetY left+close: steering suppressed");
    TEST_ASSERT(r.throttle == PWM_NEUTRAL || r.throttle < (PWM_NEUTRAL + 150),
                "feetY left+close: throttle near neutral");
}

// ─── PWM 值始终在合法范围 ───
void test_pwm_bounds() {
    printf("[TEST] All outputs within 1000-2000 μs\n");
    FollowLogic fl;
    struct TestCase { float ds; int cx; int fy; bool hp; };
    TestCase cases[] = {
        {0.10f, 0,   100, true},   // far left
        {0.10f, 191, 100, true},   // far right
        {0.50f, 0,   100, true},   // mid left
        {0.50f, 191, 100, true},   // mid right
        {0.80f, 96,  100, true},   // near center
        {0.95f, 96,  100, true},   // very near
        {0.0f,  0,    50, true},   // feetY far left
        {0.0f,  191, 180, true},   // feetY near right
        {0.0f,  96,    0, false},  // no person
    };
    for (auto& tc : cases) {
        MotorCmd r = fl.update(tc.hp, tc.cx, tc.fy, tc.ds);
        if (r.throttle < PWM_MIN || r.throttle > PWM_MAX) {
            printf("[FAIL] throttle=%u out of bounds (ds=%.2f cx=%d fy=%d)\n",
                   r.throttle, tc.ds, tc.cx, tc.fy);
            tests_failed++;
            return;
        }
        if (r.steering < PWM_MIN || r.steering > PWM_MAX) {
            printf("[FAIL] steering=%u out of bounds (ds=%.2f cx=%d fy=%d)\n",
                   r.steering, tc.ds, tc.cx, tc.fy);
            tests_failed++;
            return;
        }
    }
    TEST_ASSERT(true, "All PWM outputs within 1000-2000 μs range");
}

// ─── 转向量与偏移成正比 ───
void test_steering_proportional() {
    printf("[TEST] Steering proportional to offset\n");
    FollowLogic fl;
    // 小偏移 → 小转向
    MotorCmd r1 = fl.update(true, 80, 100, 0.40f);   // offset = -16
    // 大偏移 → 大转向
    MotorCmd r2 = fl.update(true, 30, 100, 0.40f);   // offset = -66

    int steer1 = abs((int)r1.steering - (int)PWM_NEUTRAL);
    int steer2 = abs((int)r2.steering - (int)PWM_NEUTRAL);
    TEST_ASSERT(steer2 > steer1,
                "Larger offset produces larger steering magnitude");
}

int main() {
    printf("=== FollowLogic Tests (HC6060A Mixed-Mode) ===\n\n");
    test_no_person();
    test_dist_stop();
    test_dist_far_center();
    test_person_left_steer_left();
    test_person_right_steer_right();
    test_deadband_center();
    test_medium_distance();
    test_near_slow_no_steer();
    test_feety_stop();
    test_feety_far();
    test_feety_left_close();
    test_pwm_bounds();
    test_steering_proportional();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
