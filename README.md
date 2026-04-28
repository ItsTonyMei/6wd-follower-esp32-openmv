# 6WD Follower — ESP32 + OpenMV

Six-wheel robot car that autonomously follows a person using machine vision and ultrasonic obstacle avoidance.

## Architecture

Two-board design: **ESP32** (motor control, ultrasonics, dashboard) + **OpenMV** (camera, YOLO person detection).

```
OpenMV → UART1 → ESP32 VisionTask → FollowLogic → MotorCmd queue → MotorTask → MotorDriver
```

| Board | MCU | Role |
|-------|-----|------|
| UniBoard | ESP32 | Motor PWM (LEDC), ultrasonic HC-SR04 ×2, WiFi AP dashboard, FreeRTOS tasks |
| OpenMV Cam | STM32H7 | RGB565 camera, YOLO LC person detection, UART VIS protocol |

## Directory Structure

```
├── ESP32_Solo/          # ESP32 firmware (Arduino/ESP-IDF)
│   ├── ESP32_Solo.ino   # Main entry, creates FreeRTOS tasks
│   ├── Config.h         # Unified pin assignments and parameters
│   ├── MotorDriver.*    # LEDC PWM motor control (RZ7886)
│   ├── UltrasonicSensors.*  # Dual HC-SR04 with obstacle detection
│   ├── MotorTask.*      # Obstacle avoidance state machine + motor control task
│   ├── VisionBridge.*   # OpenMV UART1 protocol parser
│   ├── FollowLogic.*    # Person-following decision logic
│   ├── DashboardServer.*  # WiFi AP + web dashboard
│   └── ...
├── OpenMV/              # OpenMV camera firmware (MicroPython)
│   ├── main.py          # Main loop
│   ├── person_detector.py  # YOLO LC person detection
│   ├── settings.py      # All tunable parameters
│   ├── debug_draw.py    # On-screen debug overlay
│   └── yolo_lc_192.tflite  # TFLite model (Git LFS)
├── Test/                # Unit tests
│   ├── ESP32/           # PlatformIO test_xxx.cpp files
│   └── OpenMV/          # Python test scripts
└── docs/                # Documentation
    ├── ARCHITECTURE.md
    ├── WIRING.md
    └── MULTI_AGENT_TASKS.md
```

## Key Features

- **Person following** — YOLO LC model detects person position, ESP32 calculates motor commands based on distance and lateral offset
- **Obstacle avoidance** — Dual ultrasonics with danger zone (STOP) and warning zone (redirect) state machine
- **Safety timeouts** — Command timeout 500 ms, vision timeout 700 ms, avoidance timeout 2 s
- **WiFi dashboard** — Real-time telemetry via browser on ESP32 AP

## Hardware

- ESP32 Dev Module (UniBoard)
- OpenMV Cam H7 Plus
- 2× HC-SR04 ultrasonic sensors
- 2× RZ7886 motor drivers (6WD platform)
- 5V regulator + battery

## Setup

### ESP32

Open `ESP32_Solo/ESP32_Solo.ino` in Arduino IDE or PlatformIO. Install required libraries (`WiFi`, `esp_task_wdt`, FreeRTOS). Edit WiFi credentials in `Config.h`.

### OpenMV

Copy `OpenMV/` contents to OpenMV Cam. Ensure `yolo_lc_192.tflite` is present (tracked via Git LFS).

## Git LFS

The TFLite model file (`OpenMV/yolo_lc_192.tflite`) is stored via Git LFS. Install Git LFS before cloning:

```bash
git lfs install
git clone https://github.com/ItsTonyMei/6wd-follower-esp32-openmv.git
```
