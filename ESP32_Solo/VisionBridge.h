#pragma once

#include <Arduino.h>
#include <cstring>
#include "Config.h"

// ============================================================================
// VisionBridge: OpenMV UART1 VIS 协议解析器
// VisionBridge: parses OpenMV VIS protocol frames from UART1 (RX=GPIO15)
//
// 协议格式: VIS:cx,cy,w,h,feetY,conf,TYPE,distScore*checksum\r\n
//   例: VIS:96,96,50,120,155,0.85,PERSON,0.75*122\r\n
//   无人: VIS:0,0,0,0,0,0.00,NONE,0.00*124\r\n
// ============================================================================
class VisionBridge {
public:
    void begin();

    // 每循环调用一次 — 从 UART1 (GPIO15) 读取并解析
    void handle();

    // 最新解析结果查询
    bool hasValidReading() const { return valid_ && isFresh(); }
    bool hasPerson() const {
        return hasValidReading() && strcmp(type_, "PERSON") == 0 && conf_ > 0.0f;
    }
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

    // 数据是否在 VISION_TIMEOUT_MS 内有效
    bool isFresh() const { return (millis() - lastUpdateMs_) <= VISION_TIMEOUT_MS; }

    bool valid_ = false;
    int cx_ = 0, cy_ = 0, w_ = 0, h_ = 0, feetY_ = 0;
    float conf_ = 0.0f;
    float distScore_ = 0.0f;
    char type_[32] = "";
    unsigned long lastUpdateMs_ = 0;

    // UART 接收 buffer
    char rxBuf_[128] = {0};
    int rxLen_ = 0;
};
