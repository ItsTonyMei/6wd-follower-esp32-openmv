#include "VisionBridge.h"
#include "Config.h"
#include "ProtocolUtils.h"

// UART1: RX=GPIO15, TX=GPIO4 (TX not used)
// OpenMV sends VIS:cx,cy,w,h,feet_y,conf,TYPE,dist_score\r\n
#define VIS_UART Serial1

void VisionBridge::begin() {
    VIS_UART.begin(115200, SERIAL_8N1, 15, 4);
    Serial.println("[VisionBridge] UART1 started: RX=GPIO15 @ 115200");
}

void VisionBridge::reset() {
    rxLen_ = 0;
    rxBuf_[0] = '\0';
    valid_ = false;
}

void VisionBridge::handle() {
    while (VIS_UART.available()) {
        char c = VIS_UART.read();
        if (c == '\n' || c == '\r') {
            if (rxLen_ >= 4 && rxBuf_[0] == 'V' && rxBuf_[1] == 'I' && rxBuf_[2] == 'S' && rxBuf_[3] == ':') {
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

    const char* star = strchr(buf, '*');
    int starPos = star ? (star - buf) : -1;

    // Find all 7 comma positions
    const char* p[7];
    p[0] = strchr(buf + 4, ',');
    for (int i = 1; i < 7; i++) {
        if (!p[i-1]) return false;
        p[i] = strchr(p[i-1] + 1, ',');
    }
    if (!p[6]) return false;

    // Extract integers by null-terminating temporarily
    char orig;
    int pos[7];
    for (int i = 0; i < 7; i++) pos[i] = p[i] - buf;

    orig = buf[pos[0]]; const_cast<char*>(buf)[pos[0]] = '\0';
    cx_ = atoi(buf + 4); const_cast<char*>(buf)[pos[0]] = orig;

    orig = buf[pos[1]]; const_cast<char*>(buf)[pos[1]] = '\0';
    cy_ = atoi(buf + pos[0] + 1); const_cast<char*>(buf)[pos[1]] = orig;

    orig = buf[pos[2]]; const_cast<char*>(buf)[pos[2]] = '\0';
    w_ = atoi(buf + pos[1] + 1); const_cast<char*>(buf)[pos[2]] = orig;

    orig = buf[pos[3]]; const_cast<char*>(buf)[pos[3]] = '\0';
    h_ = atoi(buf + pos[2] + 1); const_cast<char*>(buf)[pos[3]] = orig;

    orig = buf[pos[4]]; const_cast<char*>(buf)[pos[4]] = '\0';
    feetY_ = atoi(buf + pos[3] + 1); const_cast<char*>(buf)[pos[4]] = orig;

    orig = buf[pos[5]]; const_cast<char*>(buf)[pos[5]] = '\0';
    conf_ = atof(buf + pos[4] + 1); const_cast<char*>(buf)[pos[5]] = orig;

    orig = buf[pos[6]]; const_cast<char*>(buf)[pos[6]] = '\0';
    safeStrCopy(type_, buf + pos[5] + 1, sizeof(type_));
    const_cast<char*>(buf)[pos[6]] = orig;

    // Extract distScore
    if (starPos > pos[6] + 1) {
        orig = buf[starPos]; const_cast<char*>(buf)[starPos] = '\0';
        distScore_ = atof(buf + pos[6] + 1);
        const_cast<char*>(buf)[starPos] = orig;
    } else {
        const char* nextComma = strchr(buf + pos[6] + 1, ',');
        distScore_ = nextComma ? atof(nextComma + 1) : atof(buf + pos[6] + 1);
    }

    return true;
}
