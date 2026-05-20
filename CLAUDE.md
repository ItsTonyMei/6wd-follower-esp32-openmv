# CLAUDE.md — 6WD Follower ESP32 + OpenMV

## Project Overview

六轮机器人小车: ESP32 (UniBoard) 负责电机 + 超声波 + Dashboard; OpenMV Cam 负责摄像头 + YOLO person detection。OpenMV 通过 UART1 单向发送 VIS 数据给 ESP32。

## Language Convention

**注释和文档使用中文 + 英文专业名词。** 技术术语 (YOLO, PWM, FreeRTOS, UART, GPIO, LEDC, JSON, checksum, H-bridge, HC-SR04, model, frame, packet 等) 保持英文；解释性文字、架构说明、注意事项使用中文。

## Build System

- **ESP32**: Arduino IDE 或 PlatformIO。入口: `ESP32_Solo/ESP32_Solo.ino`。基于 Arduino framework (底层 ESP-IDF)。
- **OpenMV**: MicroPython，直接运行于 OpenMV Cam。无编译步骤 — 将 `.py` 文件复制到摄像头。

## Architecture Conventions

- FreeRTOS tasks 运行于双核 ESP32: MotorTask + VisionTask on Core 0, WebTask on Core 1。
- Task 间通信通过 FreeRTOS queue (`MotorCmd` struct, 2 bytes)。
- 主循环无动态分配 — 固定大小 queue 和栈分配 struct。
- `Config.h` 是引脚、阈值、task 参数的单一数据源 (single source of truth)。
- 避障运行于 MotorTask 内部，优先级链: 危险区 STOP → 命令超时 → 警告区转向 → 正常跟随。
- `DataAggregator` 封装互斥锁，统一管理线程安全的 CarState + VisState 读写。

## Code Style

- C++ for ESP32 (`.cpp`/`.h`), MicroPython for OpenMV (`.py`)。
- 引脚分配为 `constexpr` in `Config.h`。
- Motor command 为 `enum class CarCmd : uint8_t`。
- `FollowLogic::update()` 直接返回 `MotorCmd` struct，不再使用字符串协议。
- OpenMV 使用 assert 验证配置参数阈值。

## Testing

- ESP32 unit tests in `Test/ESP32/` — PC 端 GCC 编译, `Test/ArduinoMock.h` 提供类型桩。
- OpenMV tests in `Test/OpenMV/` — PC 端 Python/pytest 运行, 使用纯函数副本避免硬件依赖。

## Available Agents & Skills

- `cortex-debugger`: ESP32 firmware crash analysis (Guru Meditation, stack overflow, FreeRTOS deadlocks, task watchdog resets)
- `protocol-analyzer`: UART protocol debugging (VIS: ASCII frames, XOR checksum verification, framing errors)
- `esp32-firmware-engineer`: ESP-IDF specific guidance (build/flash/monitor, partition tables, sdkconfig, power optimization)

## Git LFS

仅 `*.tflite` 文件由 Git LFS 追踪。所有源代码为常规 Git objects。
