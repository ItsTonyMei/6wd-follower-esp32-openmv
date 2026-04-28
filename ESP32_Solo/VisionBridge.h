#pragma once

#include <Arduino.h>
#include <cstring>
#include "Config.h"

class VisionBridge {
public:
    void begin();

    // Call every loop — reads from UART1 (GPIO15)
    void handle();

    // Get latest parsed vision state
    bool hasValidReading() const { return valid_ && isFresh(); }
    bool hasPerson() const { return hasValidReading() && strcmp(type_, "PERSON") == 0 && conf_ > 0.0f; }
    int cx() const { return cx_; }
    int cy() const { return cy_; }
    int w() const { return w_; }
    int h() const { return h_; }
    int feetY() const { return feetY_; }
    float confidence() const { return conf_; }
    float distScore() const { return distScore_; }
    const char* type() const { return type_; }

private:
    bool parseVisionPacket(const char* buf, size_t len);
    void reset();

    bool isFresh() const { return (millis() - lastUpdateMs_) <= VISION_TIMEOUT_MS; }

    bool valid_ = false;
    int cx_ = 0, cy_ = 0, w_ = 0, h_ = 0, feetY_ = 0;
    float conf_ = 0.0f;
    float distScore_ = 0.0f;
    char type_[32] = "";
    unsigned long lastUpdateMs_ = 0;

    char rxBuf_[128] = {0};
    int rxLen_ = 0;
};
