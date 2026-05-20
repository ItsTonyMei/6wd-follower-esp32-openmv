#include "VisionBridge.h"
#include "Config.h"
#include "ProtocolUtils.h"

// UART1: RX=GPIO15, TX=GPIO4 (TX 未使用, 单向接收 OpenMV 数据)
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
            // 检测 "VIS:" 帧头
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
    // XOR checksum 验证（含 '*' 则校验，无 '*' 向后兼容）
    if (!verifyChecksum(buf, 4)) {
        return false;
    }

    const char* p = buf + 4;  // 跳过 "VIS:" 前缀
    char* end = nullptr;

    // 使用 strtol/strtof 的 endptr 参数逐字段解析，避免 const_cast<char*> 修改 buffer
    // Parse each comma-separated field using strtol/strtof with endptr

    cx_    = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    cy_    = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    w_     = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    h_     = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    feetY_  = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    conf_  = strtof(p, &end);           if (end == p || *end != ',') return false;  p = end + 1;

    // 提取 type 字符串 ("PERSON" 或 "NONE")，直到下一个 ',' 或 '*'
    const char* typeEnd = strpbrk(p, ",*");
    if (!typeEnd) return false;
    size_t tLen = typeEnd - p;
    if (tLen >= sizeof(type_)) tLen = sizeof(type_) - 1;
    memcpy(type_, p, tLen);
    type_[tLen] = '\0';
    p = typeEnd;
    if (*p == ',') p++;

    // 解析 distScore (最后的数值字段，之后可能是 '*' checksum 或 '\0')
    distScore_ = strtof(p, &end);
    // end 指向 '*' 或 '\0' — 两者均可接受

    return true;
}
