# CLAUDE.md — 6WD Follower ESP32 + OpenMV

## Project Overview

Six-wheel robot car: ESP32 (UniBoard) handles motors + ultrasonics + dashboard; OpenMV Cam handles camera + YOLO person detection. OpenMV sends vision data to ESP32 over UART1.

## Build System

- **ESP32**: Arduino IDE or PlatformIO. Entry point: `ESP32_Solo/ESP32_Solo.ino`. Uses Arduino framework on ESP32 (ESP-IDF under the hood).
- **OpenMV**: MicroPython, runs directly on OpenMV Cam. No build step — copy `.py` files to the camera.

## Architecture Conventions

- FreeRTOS tasks run on dual-core ESP32: MotorTask + VisionTask on Core 0, WebTask on Core 1.
- Inter-task communication via FreeRTOS queues (`MotorCmd` struct, 2 bytes).
- No dynamic allocation in the main loop — fixed-size queues and stack-allocated structs.
- `Config.h` is the single source of truth for pins, thresholds, and task parameters.
- Obstacle avoidance runs inside MotorTask with a priority chain: danger STOP → command timeout → warning redirect → normal follow.

## Code Style

- C++ for ESP32 (`.cpp`/`.h`), MicroPython for OpenMV (`.py`).
- Pin assignments as `constexpr` in `Config.h`.
- Motor commands as `enum class CarCmd : uint8_t`.
- OpenMV uses validation helpers (`validate_range`, `validate_positive`) for configuration parameters.

## Testing

- ESP32 unit tests in `Test/ESP32/` — compile with PlatformIO test framework.
- OpenMV tests in `Test/OpenMV/` — run on desktop Python with mocked `sensor`/`pyb` modules.

## Git LFS

Only `*.tflite` files are tracked by Git LFS. All source code is regular Git objects.
