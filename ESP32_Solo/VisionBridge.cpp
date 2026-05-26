#include "VisionBridge.h"
#include "Config.h"
#include "ProtocolUtils.h"

// Serial2: default RX=GPIO16 (← N6 P0 SW UART), 9600 baud
#define VIS_UART Serial2

void VisionBridge::begin() {
    VIS_UART.begin(4800);
    Serial.printf("[VisionBridge] Serial2 RX=GPIO%d @ 4800\n", PIN_VIS_RX);
}

void VisionBridge::reset() {
    rxLen_ = 0;
    rxBuf_[0] = '\0';
    valid_ = false;
}

void VisionBridge::handle() {
    // 直接 read(), 每次最多处理 80 字节防止死循环
    int c, limit = 80;
    static unsigned long dbgMs = 0;
    while (limit-- > 0 && (c = VIS_UART.read()) >= 0 && c < 256) {
        if (millis() - dbgMs > 1000) {
            Serial.printf("[VIS] rx active\n");
            dbgMs = millis();
        }
        if (c == '\n' || c == '\r') {
            if (rxLen_ >= 4 && rxBuf_[0] == 'V' && rxBuf_[1] == 'I'
                && rxBuf_[2] == 'S' && rxBuf_[3] == ':') {
                rxBuf_[rxLen_] = '\0';
                if (parseVisionPacket(rxBuf_, rxLen_)) {
                    lastUpdateMs_ = millis();
                    valid_ = true;
                }
            }
            reset();
        } else if (rxLen_ < (int)sizeof(rxBuf_) - 1) {
            rxBuf_[rxLen_++] = c;
        }
    }
}

bool VisionBridge::parseVisionPacket(const char* buf, size_t /*len*/) {
    if (!verifyChecksum(buf, 4)) {
        return false;
    }

    const char* p = buf + 4;
    char* end = nullptr;

    cx_    = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    cy_    = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    w_     = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    h_     = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    feetY_  = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    conf_  = strtof(p, &end);           if (end == p || *end != ',') return false;  p = end + 1;

    const char* typeEnd = strpbrk(p, ",*");
    if (!typeEnd) return false;
    size_t tLen = typeEnd - p;
    if (tLen >= sizeof(type_)) tLen = sizeof(type_) - 1;
    memcpy(type_, p, tLen);
    type_[tLen] = '\0';
    p = typeEnd;
    if (*p == ',') p++;

    distScore_ = strtof(p, &end);

    tofDist_ = 0;
    if (*end == ',') {
        p = end + 1;
        tofDist_ = (int)strtol(p, &end, 10);
    }

    return true;
}
