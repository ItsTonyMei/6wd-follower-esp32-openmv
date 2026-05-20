# ESP32 Firmware Unit Tests

PC 端编译运行 (需 GCC/Clang, 无需 ESP32 硬件)

## Build & Run

### Linux / Mac
```bash
cd Test/ESP32
g++ -std=c++17 -o VisionBridge_test VisionBridge_test.cpp -I. -I../../ESP32_Solo && ./VisionBridge_test
g++ -std=c++17 -o FollowLogic_test FollowLogic_test.cpp -I. -I../../ESP32_Solo && ./FollowLogic_test
g++ -std=c++17 -o MotorDriver_test MotorDriver_test.cpp -I. -I../../ESP32_Solo && ./MotorDriver_test
```

### Windows (MSYS2 / MinGW)
```bash
cd Test/ESP32
g++ -std=c++17 -o VisionBridge_test.exe VisionBridge_test.cpp -I. -I../../ESP32_Solo && ./VisionBridge_test.exe
g++ -std=c++17 -o FollowLogic_test.exe FollowLogic_test.cpp -I. -I../../ESP32_Solo && ./FollowLogic_test.exe
g++ -std=c++17 -o MotorDriver_test.exe MotorDriver_test.cpp -I. -I../../ESP32_Solo && ./MotorDriver_test.exe
```

## Test Files

| File | Target | Description |
|------|--------|-------------|
| `VisionBridge_test.cpp` | VIS 协议解析 | VIS packet parsing, XOR checksum, field extraction |
| `FollowLogic_test.cpp` | 跟随决策 | MotorCmd output, distScore/feetY fallback, turn decisions |
| `MotorDriver_test.cpp` | 电机驱动 | LEDC PWM output, dead zone, direction control |

## Mock 环境

`Test/ArduinoMock.h` 提供 ESP32 Arduino 框架的 PC 端类型桩 (type stubs)。测试编译时通过 `-ITest` 将 `Arduino.h` 重定向为 mock。
