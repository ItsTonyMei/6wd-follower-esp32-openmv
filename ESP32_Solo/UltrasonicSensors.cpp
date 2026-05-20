#include "UltrasonicSensors.h"
#include "Config.h"

void UltrasonicSensors::begin() {
    pinMode(PIN_US_LEFT_TRIG,  OUTPUT);
    pinMode(PIN_US_LEFT_ECHO,  INPUT);
    pinMode(PIN_US_RIGHT_TRIG, OUTPUT);
    pinMode(PIN_US_RIGHT_ECHO, INPUT);
    digitalWrite(PIN_US_LEFT_TRIG,  LOW);
    digitalWrite(PIN_US_RIGHT_TRIG, LOW);
}

// 每 50ms 交替读取左右传感器，避免声波串扰
bool UltrasonicSensors::update() {
    const unsigned long now = millis();
    if (now - lastReadMs_ < ULTRASONIC_INTERVAL_MS) {
        return false;
    }
    lastReadMs_ = now;

    if (readLeftNext_) {
        readings_.leftCm  = readDistanceCm(PIN_US_LEFT_TRIG,  PIN_US_LEFT_ECHO);
    } else {
        readings_.rightCm = readDistanceCm(PIN_US_RIGHT_TRIG, PIN_US_RIGHT_ECHO);
    }
    readLeftNext_ = !readLeftNext_;
    return true;
}

UltrasonicReadings UltrasonicSensors::readings() const {
    return readings_;
}

uint8_t UltrasonicSensors::obstacleSide() const {
    int l = readings_.leftCm;
    int r = readings_.rightCm;

    // 0 = 极近 (danger), >= OBSTACLE_WARN_CM = 安全
    bool leftWarn  = l >= 0 && l < OBSTACLE_WARN_CM;
    bool rightWarn = r >= 0 && r < OBSTACLE_WARN_CM;

    if (leftWarn && rightWarn) return US_SIDE_BOTH;
    if (leftWarn)              return US_SIDE_LEFT;
    if (rightWarn)             return US_SIDE_RIGHT;
    return US_SIDE_NONE;
}

// HC-SR04 标准 trig/echo 测距序列
int UltrasonicSensors::readDistanceCm(uint8_t trigPin, uint8_t echoPin) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    const unsigned long duration = pulseIn(echoPin, HIGH, ULTRASONIC_TIMEOUT_US);
    if (duration == 0) {
        return ULTRASONIC_MAX_CM;   // 超时 → 视为无障碍
    }

    // 声速 340m/s → 往返 1cm ≈ 58μs
    const int distance = static_cast<int>(duration / 58UL);
    return constrain(distance, 0, ULTRASONIC_MAX_CM);
}
