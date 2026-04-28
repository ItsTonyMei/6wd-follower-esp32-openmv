#pragma once

#include <Arduino.h>

struct CarState {
    bool valid;
    int leftPwm;
    int rightPwm;
    int leftUltrasonic;
    int rightUltrasonic;
    char action[8];
    unsigned long timestamp;
};

struct VisState {
    bool valid;
    int cx;
    int cy;
    int w;
    int h;
    float confidence;
    char type[32];
    float distScore;
    int feetY;
    bool hasPerson;
    unsigned long timestamp;
};

class DataAggregator {
public:
    void begin();

    void updateCar(const CarState& state);
    void updateVis(const VisState& state);

    CarState getCar() const;
    VisState getVis() const;

    // Returns combined JSON for web dashboard
    String getJson() const;

private:
    CarState carState_ = {false, 0, 0, 0, 0, "", 0};
    VisState visState_ = {false, 0, 0, 0, 0, 0.0f, "", 0.0f, 0, false, 0};

    mutable unsigned long lastJsonMs_ = 0;
    mutable String cachedJson_;
};
