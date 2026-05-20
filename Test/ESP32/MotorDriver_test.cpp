// ============================================================================
// MotorDriver 单元测试
// 测试: LEDC PWM 输出 / 死区保护 / 方向控制
// ============================================================================

#include <cstdio>
#include <cstring>
#include "MotorDriver.h"

// ─── Mock Arduino/ESP32 LEDC functions for PC testing ───
static struct LEDCPinState {
    uint32_t duty = 0;
    bool attached = false;
} pinState[4];

constexpr uint8_t PIN_MAP[] = {25, 26, 27, 19};  // match Config.h

static int pinIndex(uint8_t pin) {
    for (int i = 0; i < 4; i++)
        if (PIN_MAP[i] == pin) return i;
    return -1;
}

// 使用 ESP32 Arduino core 3.x API: ledcAttach / ledcWrite
void ledcAttach(uint8_t pin, uint32_t freq, uint8_t resolution) {
    int i = pinIndex(pin);
    if (i >= 0) pinState[i].attached = true;
    (void)freq; (void)resolution;
}
void ledcWrite(uint8_t pin, uint32_t duty) {
    int i = pinIndex(pin);
    if (i >= 0) pinState[i].duty = duty;
}
uint32_t ledcRead(uint8_t pin) {
    int i = pinIndex(pin);
    return (i >= 0) ? pinState[i].duty : 0;
}

// ─── Test helpers ───
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (cond) { printf("[PASS] %s\n", msg); tests_passed++; } \
        else { printf("[FAIL] %s\n", msg); tests_failed++; } \
    } while(0)

// Test: begin() 初始化所有 4 个 LEDC channel
void test_init() {
    printf("[TEST] MotorDriver begin() attaches all channels\n");
    MotorDriver motor;
    motor.begin();

    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(pinState[i].attached, "Channel attached to pin");
    }
}

// Test: stop() 将所有 PWM duty 清零
void test_stop() {
    printf("[TEST] MotorDriver stop() zeros all duties\n");
    MotorDriver motor;
    motor.begin();
    motor.forward(150);
    motor.stop();

    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(pinState[i].duty == 0, "All duties zero after stop()");
    }
}

// Test: forward(power) 设置前进 PWM，左右对称
void test_forward() {
    printf("[TEST] Motor forward() symmetrical PWM\n");
    MotorDriver motor;
    motor.begin();
    motor.forward(120);

    // 左前进 = 120, 右前进 = 120, 左右后退均 = 0
    uint32_t leftFwd  = ledcRead(PIN_MAP[0]);  // PIN_LEFT_FORWARD  = 25
    uint32_t leftRev  = ledcRead(PIN_MAP[1]);  // PIN_LEFT_REVERSE  = 26
    uint32_t rightFwd = ledcRead(PIN_MAP[2]);  // PIN_RIGHT_FORWARD = 27
    uint32_t rightRev = ledcRead(PIN_MAP[3]);  // PIN_RIGHT_REVERSE = 19

    TEST_ASSERT(leftFwd == 120, "Left forward = 120");
    TEST_ASSERT(leftRev == 0, "Left reverse = 0");
    TEST_ASSERT(rightFwd == 120, "Right forward = 120");
    TEST_ASSERT(rightRev == 0, "Right reverse = 0");
}

// Test: rotateLeft() → 左轮反转, 右轮正转
void test_rotate_left() {
    printf("[TEST] Motor rotateLeft() opposite directions\n");
    MotorDriver motor;
    motor.begin();
    motor.rotateLeft(100);

    uint32_t leftFwd  = ledcRead(PIN_MAP[0]);
    uint32_t leftRev  = ledcRead(PIN_MAP[1]);
    uint32_t rightFwd = ledcRead(PIN_MAP[2]);
    uint32_t rightRev = ledcRead(PIN_MAP[3]);

    TEST_ASSERT(leftFwd == 0, "Left forward = 0 on rotateLeft");
    TEST_ASSERT(leftRev == 100, "Left reverse = 100 on rotateLeft");
    TEST_ASSERT(rightFwd == 100, "Right forward = 100 on rotateLeft");
    TEST_ASSERT(rightRev == 0, "Right reverse = 0 on rotateLeft");
}

// Test: drive() 直接控制支持负值 (reverse)
void test_drive_reverse() {
    printf("[TEST] Motor drive() negative power = reverse\n");
    MotorDriver motor;
    motor.begin();
    motor.drive(-80, -80);

    uint32_t leftRev  = ledcRead(PIN_MAP[1]);  // PIN_LEFT_REVERSE
    uint32_t rightRev = ledcRead(PIN_MAP[3]);  // PIN_RIGHT_REVERSE

    TEST_ASSERT(leftRev == 80, "Left reverse = 80");
    TEST_ASSERT(rightRev == 80, "Right reverse = 80");
}

int main() {
    printf("=== MotorDriver Tests ===\n\n");
    test_init();
    test_stop();
    test_forward();
    test_rotate_left();
    test_drive_reverse();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
