#include "DataAggregator.h"
#include "FollowLogic.h"

void DataAggregator::begin() {}

// Escape JSON string: handle quotes, backslashes, and control characters
static String escapeJson(const String& s) {
    String result;
    result.reserve(s.length() * 2);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '"' || c == '\\') {
            result += '\\';
            result += c;
        } else if (c == '\n') {
            result += '\\';
            result += 'n';
        } else if (c == '\r') {
            result += '\\';
            result += 'r';
        } else if (c == '\t') {
            result += '\\';
            result += 't';
        } else if (c >= 0x00 && c <= 0x1F) {
            result += "\\u00";
            result += "0123456789ABCDEF"[(c >> 4) & 0x0F];
            result += "0123456789ABCDEF"[c & 0x0F];
        } else {
            result += c;
        }
    }
    return result;
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

String DataAggregator::getJson() const {
    unsigned long now = millis();

    // Cache JSON for 50ms to avoid spam
    if (now - lastJsonMs_ < 50) {
        return cachedJson_;
    }

    lastJsonMs_ = now;

    String json;
    json.reserve(256);
    json = "{";
    json += "\"car\":{";
    json += "\"v\":" + String(carState_.valid ? 1 : 0) + ",";
    json += "\"l\":" + String(carState_.leftPwm) + ",";
    json += "\"r\":" + String(carState_.rightPwm) + ",";
    json += "\"ul\":" + String(carState_.leftUltrasonic) + ",";
    json += "\"ur\":" + String(carState_.rightUltrasonic) + ",";
    json += "\"act\":\"" + escapeJson(carState_.action) + "\",";
    json += "\"ts\":" + String(carState_.timestamp);
    json += "},";

    json += "\"vis\":{";
    json += "\"v\":" + String(visState_.valid ? 1 : 0) + ",";
    json += "\"hp\":" + String(visState_.hasPerson ? 1 : 0) + ",";
    json += "\"cx\":" + String(visState_.cx) + ",";
    json += "\"cy\":" + String(visState_.cy) + ",";
    json += "\"w\":" + String(visState_.w) + ",";
    json += "\"h\":" + String(visState_.h) + ",";
    json += "\"conf\":" + String(visState_.confidence, 2) + ",";
    json += "\"type\":\"" + escapeJson(String(visState_.type)) + "\",";
    json += "\"ds\":" + String(visState_.distScore, 2) + ",";
    json += "\"fy\":" + String(visState_.feetY) + ",";
    json += "\"ts\":" + String(visState_.timestamp);
    json += "},";

    json += "\"snap\":0,";

    // Thresholds from FollowLogic (must match frontend for correct color mapping)
    json += "\"cfg\":{";
    json += "\"ds_stop\":" + String(FollowLogic::DIST_SCORE_STOP, 2) + ",";
    json += "\"ds_slow\":" + String(FollowLogic::DIST_SCORE_SLOW, 2) + ",";
    json += "\"ds_far\":" + String(FollowLogic::DIST_SCORE_FAR, 2);
    json += "}";
    json += "}";

    cachedJson_ = json;
    return json;
}
