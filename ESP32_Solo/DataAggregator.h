#pragma once

#include <Arduino.h>
#include <freertos/semphr.h>

// ============================================================================
// CarState: 车辆运行状态（MotorTask 写入）
// CarState: vehicle running state (written by MotorTask)
// ============================================================================
struct CarState {
    bool valid;
    int leftPwm;            // 左轮 PWM (0-255)
    int rightPwm;           // 右轮 PWM (0-255)
    char action[8];         // 当前动作: STOP / FWD / LFT / RGT
    unsigned long timestamp;
};

// ============================================================================
// VisState: OpenMV 视觉检测结果（VisionTask 写入）
// VisState: vision detection result (written by VisionTask)
// ============================================================================
struct VisState {
    bool valid;
    int cx;                 // 检测框中心 X (0-191, model 坐标)
    int cy;                 // 检测框中心 Y
    int w;                  // 检测框宽度
    int h;                  // 检测框高度
    float confidence;       // YOLO confidence (0.0-1.0)
    char type[32];          // 检测类型: "PERSON" / "NONE"
    float distScore;        // 融合距离分: 视觉特征 + VL53L1X ToF (0.0-1.0, 越大越近)
    int tofDistance;        // VL53L1X ToF 原始距离 (mm), 40-4000, 0=无效
    int feetY;              // 脚部 Y 坐标 (legacy fallback)
    bool hasPerson;         // 是否检测到人
    unsigned long timestamp;
};

// ============================================================================
// DataAggregator: 聚合 Car + Vision 状态，生成 JSON 供 Web Dashboard
// DataAggregator: aggregates Car + Vision state, generates JSON for dashboard
// ============================================================================
class DataAggregator {
public:
    void begin(SemaphoreHandle_t mutex);

    // 线程安全的状态读写 (Thread-safe state access)
    bool lock(TickType_t waitTicks = portMAX_DELAY);
    void unlock();

    void updateCar(const CarState& state);
    void updateVis(const VisState& state);

    CarState getCar() const;
    VisState getVis() const;

    // 生成组合 JSON（带 50ms cache，供 WebTask 轮询）
    // Generate combined JSON (50ms cache for WebTask polling)
    String getJson();

private:
    CarState carState_ = {false, 0, 0, "", 0};
    VisState visState_ = {false, 0, 0, 0, 0, 0.0f, "", 0.0f, 0, 0, false, 0};

    SemaphoreHandle_t mutex_ = nullptr;

    mutable unsigned long lastJsonMs_ = 0;
    mutable String cachedJson_;
};
