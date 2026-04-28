// ============================================================================
// VisionBridge Protocol Parser Tests
// Tests: VIS protocol parsing, checksum verification, data extraction
// ============================================================================

#include <cstdio>
#include <cstring>
#include <cstdlib>

// Mock verifyChecksum (copied from ProtocolUtils.cpp for standalone testing)
static bool verifyChecksum(const char* line, int prefixLen) {
    const char* star = strchr(line, '*');
    int starPos = star ? (star - line) : -1;
    if (starPos <= prefixLen) return true;  // backward-compatible
    uint8_t checksum = 0;
    for (int i = prefixLen; i < starPos; i++) {
        checksum ^= static_cast<uint8_t>(line[i]);
    }
    uint8_t expected = static_cast<uint8_t>(atoi(line + starPos + 1));
    return checksum == expected;
}

// Simulated VisionBridge parser state
struct SimVisionBridge {
    int cx_ = 0, cy_ = 0, w_ = 0, h_ = 0, feetY_ = 0;
    float conf_ = 0.0f;
    float distScore_ = 0.0f;
    char type_[32] = "";

    bool parsePacket(const char* buf) {
        if (!verifyChecksum(buf, 4)) return false;

        const char* star = strchr(buf, '*');
        int starPos = star ? (star - buf) : -1;

        const char* p[7];
        p[0] = strchr(buf + 4, ',');
        for (int i = 1; i < 7; i++) {
            if (!p[i-1]) return false;
            p[i] = strchr(p[i-1] + 1, ',');
        }
        if (!p[6]) return false;

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
        strncpy(type_, buf + pos[5] + 1, sizeof(type_) - 1);
        type_[sizeof(type_) - 1] = '\0';
        const_cast<char*>(buf)[pos[6]] = orig;

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
};

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (cond) { printf("[PASS] %s\n", msg); tests_passed++; } \
        else { printf("[FAIL] %s\n", msg); tests_failed++; } \
    } while(0)

// Test: valid VIS packet with checksum
bool test_parse_valid() {
    printf("[TEST] Parse valid VIS packet\n");
    SimVisionBridge vb;
    // VIS:96,96,50,120,155,0.85,PERSON,0.75*checksum
    // Checksum of "96,96,50,120,155,0.85,PERSON,0.75" = XOR of all chars
    const char* packet = "VIS:96,96,50,120,155,0.85,PERSON,0.75*122";
    bool ok = vb.parsePacket(packet);
    TEST_ASSERT(ok, "Valid packet parsed");
    TEST_ASSERT(vb.cx_ == 96, "cx = 96");
    TEST_ASSERT(vb.cy_ == 96, "cy = 96");
    TEST_ASSERT(vb.w_ == 50, "w = 50");
    TEST_ASSERT(vb.h_ == 120, "h = 120");
    TEST_ASSERT(vb.feetY_ == 155, "feetY = 155");
    TEST_ASSERT(vb.conf_ > 0.84f && vb.conf_ < 0.86f, "conf = 0.85");
    TEST_ASSERT(vb.distScore_ > 0.74f && vb.distScore_ < 0.76f, "distScore = 0.75");
    TEST_ASSERT(strcmp(vb.type_, "PERSON") == 0, "type = PERSON");
    return true;
}

// Test: invalid checksum
bool test_bad_checksum() {
    printf("[TEST] Invalid checksum rejected\n");
    SimVisionBridge vb;
    const char* packet = "VIS:96,96,50,120,155,0.85,PERSON,0.75*999";  // wrong checksum
    bool ok = vb.parsePacket(packet);
    TEST_ASSERT(!ok, "Bad checksum rejected");
    return true;
}

// Test: old format without checksum
bool test_no_checksum() {
    printf("[TEST] Old format (no checksum) accepted\n");
    SimVisionBridge vb;
    const char* packet = "VIS:96,96,50,120,155,0.85,PERSON,0.75";
    bool ok = vb.parsePacket(packet);
    TEST_ASSERT(ok, "Old format without *checksum accepted");
    return true;
}

// Test: NONE type (no person detected)
bool test_type_none() {
    printf("[TEST] NONE type parsing\n");
    SimVisionBridge vb;
    const char* packet = "VIS:0,0,0,0,0,0.00,NONE,0.00*XX";  // XX placeholder
    // Use correct checksum
    const char* p2 = "VIS:0,0,0,0,0,0.00,NONE,0.00*124";
    bool ok = vb.parsePacket(p2);
    TEST_ASSERT(ok, "NONE packet parsed");
    TEST_ASSERT(strcmp(vb.type_, "NONE") == 0, "type = NONE");
    TEST_ASSERT(vb.cx_ == 0, "cx = 0 for NONE");
    return true;
}

// Test: boundary values
bool test_boundary() {
    printf("[TEST] Boundary values\n");
    SimVisionBridge vb;
    const char* packet = "VIS:191,191,192,192,192,1.00,PERSON,1.00*131";
    bool ok = vb.parsePacket(packet);
    TEST_ASSERT(ok, "Max boundary values parsed");
    TEST_ASSERT(vb.cx_ == 191, "cx max");
    TEST_ASSERT(vb.feetY_ == 192, "feetY max");
    return true;
}

int main() {
    printf("=== VisionBridge Protocol Tests ===\n");
    test_parse_valid();
    test_bad_checksum();
    test_no_checksum();
    test_type_none();
    test_boundary();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}