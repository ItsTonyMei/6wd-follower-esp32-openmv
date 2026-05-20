#pragma once

#include <Arduino.h>

// ============================================================================
// UltrasonicSensors: 双 HC-SR04 超声波传感器驱动
// UltrasonicSensors: dual HC-SR04 ultrasonic sensor driver
// ============================================================================

struct UltrasonicReadings {
    int leftCm;     // 左传感器距离 (cm), -1 = invalid
    int rightCm;    // 右传感器距离 (cm), -1 = invalid
};

class UltrasonicSensors {
public:
    void begin();

    // 每 50ms 交替读取左右传感器，返回 true 表示本次有新读数
    // Alternates left/right reads every 50ms; returns true when new reading ready
    bool update();

    UltrasonicReadings readings() const;

    // 返回障碍物方向 flags:
    //   US_SIDE_NONE  = 两侧安全 (both > OBSTACLE_WARN_CM)
    //   US_SIDE_LEFT  = 左侧在警戒区
    //   US_SIDE_RIGHT = 右侧在警戒区
    //   US_SIDE_BOTH  = 两侧均在警戒区
    uint8_t obstacleSide() const;

private:
    int readDistanceCm(uint8_t trigPin, uint8_t echoPin);

    UltrasonicReadings readings_;
    unsigned long lastReadMs_ = 0;
    bool readLeftNext_ = true;
};
