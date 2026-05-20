#pragma once

#include <Arduino.h>

// ============================================================================
// VIS 协议工具: XOR checksum 验证 + 安全字符串拷贝
// Protocol utilities: XOR checksum verification + safe string copy
// ============================================================================

// 验证 line[prefixLen..starPos-1] 的 XOR checksum 是否等于 line[starPos+1..]
// 无 '*' 时视为向后兼容 (backward-compatible)，直接通过
bool verifyChecksum(const String& line, int prefixLen);
bool verifyChecksum(const char* line, int prefixLen);

// 安全字符串拷贝: 始终 null-terminate，不会溢出 destSize
static inline void safeStrCopy(char* dest, const char* src, size_t destSize) {
    if (destSize == 0) return;
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}
