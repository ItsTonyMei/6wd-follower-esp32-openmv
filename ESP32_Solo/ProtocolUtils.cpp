#include "ProtocolUtils.h"

// ─── const char* 版本 (零 heap 分配) ───
bool verifyChecksum(const char* line, int prefixLen) {
    const char* star = strchr(line, '*');
    int starPos = star ? (int)(star - line) : -1;
    if (starPos <= prefixLen) {
        // 无 '*' 或 '*' 位置过早 → 向后兼容，直接通过
        return true;
    }
    uint8_t checksum = 0;
    for (int i = prefixLen; i < starPos; i++) {
        checksum ^= static_cast<uint8_t>(line[i]);
    }
    uint8_t expected = static_cast<uint8_t>(atoi(line + starPos + 1));
    return checksum == expected;
}

// ─── String 版本 ───
bool verifyChecksum(const String& line, int prefixLen) {
    int starPos = line.indexOf('*');
    if (starPos <= prefixLen) {
        // 无 '*' 或 '*' 位置过早 → 向后兼容，直接通过
        return true;
    }
    uint8_t checksum = 0;
    for (int i = prefixLen; i < starPos; i++) {
        checksum ^= static_cast<uint8_t>(line.charAt(i));
    }
    uint8_t expected = static_cast<uint8_t>(line.substring(starPos + 1).toInt());
    return checksum == expected;
}
