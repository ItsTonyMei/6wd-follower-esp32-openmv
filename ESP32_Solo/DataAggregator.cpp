#include "DataAggregator.h"
#include "FollowLogic.h"
#include <cstdio>

void DataAggregator::begin(SemaphoreHandle_t mutex) {
    mutex_ = mutex;
}

bool DataAggregator::lock(TickType_t waitTicks) {
    if (mutex_ == nullptr) return true;
    return xSemaphoreTake(mutex_, waitTicks) == pdTRUE;
}

void DataAggregator::unlock() {
    if (mutex_ == nullptr) return;
    xSemaphoreGive(mutex_);
}

void DataAggregator::updateCar(const CarState& state) {
    carState_ = state;
}

void DataAggregator::updateVis(const VisState& state) {
    visState_ = state;
}

CarState DataAggregator::getCar() const {
    return carState_;
}

VisState DataAggregator::getVis() const {
    return visState_;
}

String DataAggregator::getJson() {
    unsigned long now = millis();

    // 50ms cache 避免高频重复生成 JSON
    if (now - lastJsonMs_ < 50) {
        return cachedJson_;
    }
    lastJsonMs_ = now;

    // 使用 snprintf 一次性格式化为固定 buffer，避免 String + 级联导致的 heap 碎片
    // Use snprintf into fixed buffer to avoid heap fragmentation from String concatenation
    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"car\":{\"v\":%d,\"l\":%d,\"r\":%d,\"act\":\"%s\",\"ts\":%lu},"
        "\"vis\":{\"v\":%d,\"hp\":%d,\"cx\":%d,\"cy\":%d,\"w\":%d,\"h\":%d,"
        "\"conf\":%.2f,\"type\":\"%s\",\"ds\":%.2f,\"tof\":%d,\"fy\":%d,\"ts\":%lu},"
        "\"snap\":0,"
        "\"cfg\":{\"ds_stop\":%.2f,\"ds_slow\":%.2f,\"ds_far\":%.2f}"
        "}",
        // car
        carState_.valid ? 1 : 0,
        carState_.leftPwm,
        carState_.rightPwm,
        carState_.action,
        carState_.timestamp,
        // vis
        visState_.valid ? 1 : 0,
        visState_.hasPerson ? 1 : 0,
        visState_.cx,
        visState_.cy,
        visState_.w,
        visState_.h,
        visState_.confidence,
        visState_.type,
        visState_.distScore,
        visState_.tofDistance,
        visState_.feetY,
        visState_.timestamp,
        // cfg (thresholds from FollowLogic, 前端颜色映射需与此一致)
        FollowLogic::DIST_SCORE_STOP,
        FollowLogic::DIST_SCORE_SLOW,
        FollowLogic::DIST_SCORE_FAR
    );

    if (len < 0 || len >= (int)sizeof(buf)) {
        cachedJson_ = "{\"error\":\"json buffer overflow\"}";
    } else {
        cachedJson_ = buf;
    }
    return cachedJson_;
}
