#include "ProtocolUtils.h"

// Zero-allocation overload for const char*
bool verifyChecksum(const char* line, int prefixLen) {
    const char* star = strchr(line, '*');
    int starPos = star ? (star - line) : -1;
    if (starPos <= prefixLen) {
        // No '*' or '*' too early: backward-compatible, accept
        return true;
    }
    uint8_t checksum = 0;
    for (int i = prefixLen; i < starPos; i++) {
        checksum ^= static_cast<uint8_t>(line[i]);
    }
    uint8_t expected = static_cast<uint8_t>(atoi(line + starPos + 1));
    return checksum == expected;
}

bool verifyChecksum(const String& line, int prefixLen) {
    int starPos = line.indexOf('*');
    if (starPos <= prefixLen) {
        // No '*' or '*' too early: backward-compatible, accept
        return true;
    }
    uint8_t checksum = 0;
    for (int i = prefixLen; i < starPos; i++) {
        checksum ^= static_cast<uint8_t>(line.charAt(i));
    }
    uint8_t expected = static_cast<uint8_t>(line.substring(starPos + 1).toInt());
    return checksum == expected;
}
