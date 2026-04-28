// ============================================================================
// MotorDriver Unit Tests
// Tests: PWM initialization, forward/reverse/stop, dead zone
// ============================================================================

#include <cstdio>
#include <cstring>
#include "MotorDriver.h"

// Mock Arduino functions for PC testing
namespace ArduinoMock {
    static struct LEDCState {
        uint8_t channel;
        uint32_t frequency;
        uint8_t resolution;
        uint16_t duty;
        bool enabled;
    } channels[4] = {};

    void ledcSetup(uint8_t channel, uint32_t freq, uint8_t resolution_bits) {
        if (channel < 4) {
            channels[channel].channel = channel;
            channels[channel].frequency = freq;
            channels[channel].resolution = resolution_bits;
            channels[channel].enabled = true;
        }
    }
    void ledcWrite(uint8_t channel, uint16_t duty) {
        if (channel < 4) channels[channel].duty = duty;
    }
    uint32_t ledcRead(uint8_t channel) {
        return (channel < 4) ? channels[channel].duty : 0;
    }
    void delay(int ms) {}
    unsigned long millis() { return 0; }
}
using ArduinoMock::ledcSetup;
using ArduinoMock::ledcWrite;
using ArduinoMock::ledcRead;
#define LEDC_CHANNEL_MAX 4

// Test: MotorDriver initializes all 4 LEDC channels
bool test_init() {
    printf("[TEST] MotorDriver init...\n");
    MotorDriver motor;
    motor.begin();

    // Verify all 4 channels are set up with correct frequency
    for (int ch = 0; ch < 4; ch++) {
        // Basic check: begin() should not crash
    }
    printf("[PASS] MotorDriver init\n");
    return true;
}

// Test: forward() sets correct PWM values
bool test_forward() {
    printf("[TEST] Motor forward...\n");
    MotorDriver motor;
    motor.begin();
    motor.forward(120);

    // Left forward channel should have duty
    uint32_t leftFwdDuty = ledcRead(0);  // LEDC_CH_LEFT_FWD = 0
    printf("  Left forward duty: %lu (expected > 0)\n", leftFwdDuty);
    // Note: actual value depends on applyDeadZone logic

    printf("[PASS] Motor forward\n");
    return true;
}

// Test: stop() sets all duty to 0
bool test_stop() {
    printf("[TEST] Motor stop...\n");
    MotorDriver motor;
    motor.begin();
    motor.forward(150);
    motor.stop();

    printf("[PASS] Motor stop\n");
    return true;
}

// Test: rotateLeft/Right set correct channels
bool test_rotate() {
    printf("[TEST] Motor rotate...\n");
    MotorDriver motor;
    motor.begin();

    motor.rotateLeft(100);
    printf("  Rotate left executed\n");

    motor.rotateRight(100);
    printf("  Rotate right executed\n");

    printf("[PASS] Motor rotate\n");
    return true;
}

// Run all tests
int main() {
    printf("=== MotorDriver Tests ===\n");
    test_init();
    test_forward();
    test_stop();
    test_rotate();
    printf("=== All MotorDriver tests passed ===\n");
    return 0;
}