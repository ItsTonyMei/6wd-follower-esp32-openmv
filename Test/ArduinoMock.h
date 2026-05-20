#pragma once
// ============================================================================
// ArduinoMock: 最小 PC 测试 mock，提供 ESP32 Arduino 框架类型桩
// Minimal PC test mock providing ESP32 Arduino framework type stubs
// ============================================================================

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>

// ─── Arduino base types ───
using uint8_t = std::uint8_t;
using uint16_t = std::uint16_t;
using uint32_t = std::uint32_t;
using int32_t = std::int32_t;
using size_t = std::size_t;

// ─── FreeRTOS types ───
using SemaphoreHandle_t = void*;
using QueueHandle_t = void*;
using TaskHandle_t = void*;
using TickType_t = uint32_t;
using BaseType_t = int;

constexpr uint32_t portMAX_DELAY = 0xFFFFFFFF;
constexpr BaseType_t pdPASS = 1;
constexpr BaseType_t pdTRUE = 1;
inline TickType_t pdMS_TO_TICKS(uint32_t ms) { return ms; }

// ─── Arduino String (minimal) ───
class String {
public:
    String() {}
    String(const char* s) : data_(s ? s : "") {}
    String(const String& o) : data_(o.data_) {}
    int indexOf(char c) const { auto p = strchr(data_.c_str(), c); return p ? (int)(p - data_.c_str()) : -1; }
    char charAt(int i) const { return data_[i]; }
    String substring(int start) const { return data_.substr(start).c_str(); }
    int toInt() const { return atoi(data_.c_str()); }
    const char* c_str() const { return data_.c_str(); }
    size_t length() const { return data_.length(); }
    void reserve(size_t) {}
    String& operator=(const char* s) { data_ = s ? s : ""; return *this; }
    String& operator=(const String& o) { data_ = o.data_; return *this; }
private:
    std::string data_;
};

// ─── Stub functions ───
inline unsigned long millis() { return 0; }
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned) {}
inline unsigned long pulseIn(uint8_t, uint8_t, unsigned long) { return 0; }

// ─── Pin constants ───
constexpr uint8_t INPUT = 0;
constexpr uint8_t OUTPUT = 1;
constexpr uint8_t LOW = 0;
constexpr uint8_t HIGH = 1;
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int constrain(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

// ─── Serial stub ───
struct SerialStub {
    void begin(unsigned long) {}
    void println() {}
    void println(const char*) {}
    void print(const char*) {}
    void print(int) {}
};
extern SerialStub Serial;
