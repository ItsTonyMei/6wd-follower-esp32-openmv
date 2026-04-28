#pragma once

#include <Arduino.h>

// Verifies XOR checksum of line[prefixLen..starPos-1] against line[starPos+1..].
// Returns true if no '*' present (backward-compatible), or if checksum matches.
bool verifyChecksum(const String& line, int prefixLen);
bool verifyChecksum(const char* line, int prefixLen);

// Safe string copy: always null-terminates dest within destSize
static inline void safeStrCopy(char* dest, const char* src, size_t destSize) {
    if (destSize == 0) return;
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}
