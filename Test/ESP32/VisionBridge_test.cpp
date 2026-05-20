// ============================================================================
// VisionBridge VIS 协议解析器测试
// 测试: VIS packet parsing, XOR checksum verification, field extraction
// 使用真实 VisionBridge.h + ProtocolUtils.h (不再内联副本)
// ============================================================================

#include <cstdio>
#include <cstring>
#include "VisionBridge.h"
#include "ProtocolUtils.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (cond) { printf("[PASS] %s\n", msg); tests_passed++; } \
        else { printf("[FAIL] %s\n", msg); tests_failed++; } \
    } while(0)

// ============================================================================
// 直接测试 VisionBridge::parseVisionPacket (通过 public handle() 不方便在 PC 测试)
// 改为直接测试协议层: verifyChecksum + 手动解析验证
// ============================================================================

// 手动解析辅助 (与 VisionBridge::parseVisionPacket 逻辑一致)
static bool parsePacket(const char* buf, int& cx, int& cy, int& w, int& h,
                        int& feetY, float& conf, char* type, size_t typeSize,
                        float& distScore) {
    if (!verifyChecksum(buf, 4)) return false;

    const char* p = buf + 4;   // skip "VIS:"
    char* end = nullptr;

    cx    = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    cy    = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    w     = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    h     = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    feetY = (int)strtol(p, &end, 10);  if (end == p || *end != ',') return false;  p = end + 1;
    conf  = strtof(p, &end);           if (end == p || *end != ',') return false;  p = end + 1;

    // type string
    const char* typeEnd = strpbrk(p, ",*");
    if (!typeEnd) return false;
    size_t tLen = typeEnd - p;
    if (tLen >= typeSize) tLen = typeSize - 1;
    memcpy(type, p, tLen);
    type[tLen] = '\0';
    p = typeEnd;
    if (*p == ',') p++;

    // distScore
    distScore = strtof(p, &end);
    return true;
}

// === 测试用例 (Test cases) ===

// 有效 VIS packet + checksum 验证
void test_parse_valid() {
    printf("[TEST] Parse valid VIS packet\n");
    // XOR of "96,96,50,120,155,0.85,PERSON,0.75" = 49
    const char* packet = "VIS:96,96,50,120,155,0.85,PERSON,0.75*49";
    int cx, cy, w, h, feetY;
    float conf, distScore;
    char type[32];

    bool ok = parsePacket(packet, cx, cy, w, h, feetY, conf, type, sizeof(type), distScore);
    TEST_ASSERT(ok, "Valid packet parsed");
    TEST_ASSERT(cx == 96, "cx = 96");
    TEST_ASSERT(cy == 96, "cy = 96");
    TEST_ASSERT(w == 50, "w = 50");
    TEST_ASSERT(h == 120, "h = 120");
    TEST_ASSERT(feetY == 155, "feetY = 155");
    TEST_ASSERT(conf > 0.84f && conf < 0.86f, "conf = 0.85");
    TEST_ASSERT(distScore > 0.74f && distScore < 0.76f, "distScore = 0.75");
    TEST_ASSERT(strcmp(type, "PERSON") == 0, "type = PERSON");
}

// 错误 checksum → 拒绝
void test_bad_checksum() {
    printf("[TEST] Invalid checksum rejected\n");
    const char* packet = "VIS:96,96,50,120,155,0.85,PERSON,0.75*999";
    int cx, cy, w, h, feetY;
    float conf, distScore;
    char type[32];

    bool ok = parsePacket(packet, cx, cy, w, h, feetY, conf, type, sizeof(type), distScore);
    TEST_ASSERT(!ok, "Bad checksum rejected");
}

// 旧格式 (无 checksum) — 向后兼容
void test_no_checksum() {
    printf("[TEST] Old format (no checksum) accepted\n");
    const char* packet = "VIS:96,96,50,120,155,0.85,PERSON,0.75";
    int cx, cy, w, h, feetY;
    float conf, distScore;
    char type[32];

    bool ok = parsePacket(packet, cx, cy, w, h, feetY, conf, type, sizeof(type), distScore);
    TEST_ASSERT(ok, "Old format without *checksum accepted");
}

// NONE type (no person detected)
void test_type_none() {
    printf("[TEST] NONE type parsing\n");
    const char* packet = "VIS:0,0,0,0,0,0.00,NONE,0.00*22";
    int cx, cy, w, h, feetY;
    float conf, distScore;
    char type[32];

    bool ok = parsePacket(packet, cx, cy, w, h, feetY, conf, type, sizeof(type), distScore);
    TEST_ASSERT(ok, "NONE packet parsed");
    TEST_ASSERT(strcmp(type, "NONE") == 0, "type = NONE");
    TEST_ASSERT(cx == 0, "cx = 0 for NONE");
}

// 边界值测试
void test_boundary() {
    printf("[TEST] Boundary values\n");
    // XOR of "191,191,192,192,192,1.00,PERSON,1.00" = 3
    const char* packet = "VIS:191,191,192,192,192,1.00,PERSON,1.00*3";
    int cx, cy, w, h, feetY;
    float conf, distScore;
    char type[32];

    bool ok = parsePacket(packet, cx, cy, w, h, feetY, conf, type, sizeof(type), distScore);
    TEST_ASSERT(ok, "Max boundary values parsed");
    TEST_ASSERT(cx == 191, "cx max");
    TEST_ASSERT(feetY == 192, "feetY max");
}

// 纯 XOR checksum 验证
void test_checksum_direct() {
    printf("[TEST] XOR checksum direct verification\n");
    // "96,96,50,120,155,0.85,PERSON,0.75" XOR checksum = 49
    TEST_ASSERT(verifyChecksum("VIS:96,96,50,120,155,0.85,PERSON,0.75*49", 4),
                "Correct checksum passes");
    TEST_ASSERT(!verifyChecksum("VIS:96,96,50,120,155,0.85,PERSON,0.75*0", 4),
                "Wrong checksum fails");
    // "0,0,0,0,0,0.00,NONE,0.00" XOR checksum = 22
    TEST_ASSERT(verifyChecksum("VIS:0,0,0,0,0,0.00,NONE,0.00*22", 4),
                "NONE packet checksum correct");
}

int main() {
    printf("=== VisionBridge Protocol Tests ===\n\n");
    test_parse_valid();
    test_bad_checksum();
    test_no_checksum();
    test_type_none();
    test_boundary();
    test_checksum_direct();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
