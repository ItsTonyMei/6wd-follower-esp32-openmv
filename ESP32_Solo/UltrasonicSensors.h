#pragma once

#include <Arduino.h>

struct UltrasonicReadings {
    int leftCm;
    int rightCm;
};

class UltrasonicSensors {
public:
    void begin();
    bool update();                      // returns true when a new reading is ready
    UltrasonicReadings readings() const;

    // Returns which side(s) have obstacles:
    //   US_SIDE_NONE  = clear (both > OBSTACLE_WARN_CM)
    //   US_SIDE_LEFT  = left side in warning/danger zone
    //   US_SIDE_RIGHT = right side in warning/danger zone
    //   US_SIDE_BOTH  = both sides in warning/danger zone
    uint8_t obstacleSide() const;

private:
    int readDistanceCm(uint8_t trigPin, uint8_t echoPin);

    UltrasonicReadings readings_;
    unsigned long lastReadMs_ = 0;
    bool readLeftNext_ = true;
};
