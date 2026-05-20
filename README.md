# 6WD Follower — ESP32 + OpenMV

六轮机器人小车：自主跟随行人（YOLO person detection + 超声波避障）。

Six-wheel robot car: autonomously follows a person using machine vision and ultrasonic obstacle avoidance.

## 架构 (Architecture)

双板设计 (Two-board design): **ESP32** (电机控制 + 超声波 + Dashboard) + **OpenMV** (摄像头 + YOLO person detection)。

```
OpenMV → UART1 → ESP32 VisionTask → FollowLogic → MotorCmd queue → MotorTask → MotorDriver
```

| Board | MCU | Role |
|-------|-----|------|
| UniBoard | ESP32 | Motor PWM (LEDC), ultrasonic HC-SR04 ×2, WiFi AP dashboard, FreeRTOS tasks |
| OpenMV Cam | STM32H7 / N6 | RGB565 camera, YOLO LC person detection, UART VIS protocol |

## 目录结构 (Directory Structure)

```
├── ESP32_Solo/              # ESP32 firmware (Arduino framework)
│   ├── ESP32_Solo.ino       # 主入口, FreeRTOS task 创建
│   ├── Config.h             # 统一配置: 引脚, 阈值, task 参数
│   ├── MotorDriver.h/cpp    # LEDC PWM 电机驱动 (RZ7886 H-bridge)
│   ├── UltrasonicSensors.h/cpp  # 双 HC-SR04 超声波 + 障碍检测
│   ├── MotorTask.h/cpp      # 避障状态机 + 电机控制 task
│   ├── VisionBridge.h/cpp   # OpenMV UART1 VIS 协议解析
│   ├── FollowLogic.h/cpp    # 人员跟随决策逻辑 → MotorCmd
│   ├── DataAggregator.h/cpp # CarState + VisState 聚合 + JSON 生成
│   ├── DashboardServer.h/cpp # WiFi AP + HTTP Dashboard
│   ├── Tasks.h/cpp          # VisionTask / WebTask FreeRTOS 实现
│   ├── ProtocolUtils.h/cpp  # XOR checksum 验证 + 安全字符串拷贝
│   └── index_html.h         # 嵌入式 Web Dashboard (SPA)
├── OpenMV/                  # OpenMV camera firmware (MicroPython)
│   ├── main.py              # 完整固件: 摄像头 + YOLO LC + VIS UART
│   └── yolo_lc_192.tflite   # TFLite model (Git LFS)
├── Test/                    # 单元测试
│   ├── ESP32/               # C++ 测试 (PC 端 GCC 编译)
│   └── OpenMV/              # Python 测试 (pytest)
└── docs/                    # 文档
    ├── ARCHITECTURE.md      # 架构说明
    └── WIRING.md            # 接线指南
```

## 核心特性 (Key Features)

- **人员跟随 (Person following)** — YOLO LC 检测人体位置，ESP32 根据距离分 (distScore) 和横向偏移计算 MotorCmd
- **多特征距离融合 (Multi-feature distance fusion)** — area_ratio + feet_y + top_y 加权融合 → distScore (0.0-1.0)
- **超声波避障 (Ultrasonic obstacle avoidance)** — 双 HC-SR04: 危险区 < 20cm → STOP, 警告区 20-40cm → 避障转向
- **安全超时 (Safety timeouts)** — 命令超时 500ms, 视觉超时 700ms, 避障超时 2s
- **WiFi Dashboard** — ESP32 AP 模式，手机浏览器直连 `http://192.168.4.1` 查看实时数据
- **双核 FreeRTOS** — MotorTask + VisionTask on Core 0, WebTask on Core 1

## 硬件 (Hardware)

- ESP32 Dev Module (UniBoard)
- OpenMV Cam H7 Plus 或 N6
- HC-SR04 超声波传感器 ×2
- RZ7886 双 H-bridge 电机驱动 (6WD 平台)
- XL2596 5V 降压模块
- 7.4V 锂电池

## 快速开始 (Quick Start)

### ESP32

1. 用 Arduino IDE 或 PlatformIO 打开 `ESP32_Solo/ESP32_Solo.ino`
2. 安装依赖库: `WiFi`, `WebServer`, `esp_task_wdt`, FreeRTOS
3. 在 `Config.h` 中修改 WiFi 密码 (`WIFI_PASS`)
4. 编译烧录到 ESP32

### OpenMV

1. 将 `OpenMV/` 目录下所有文件拷贝到 OpenMV Cam
2. 确保 `yolo_lc_192.tflite` 存在 (Git LFS 追踪)
3. N6 用户: 通过 OpenMV IDE → Tools → Edit ROM FS 上传 model

## Git LFS

TFLite model 文件 (`OpenMV/yolo_lc_192.tflite`) 通过 Git LFS 存储。克隆前请安装 Git LFS:

```bash
git lfs install
git clone https://github.com/ItsTonyMei/6wd-follower-esp32-openmv.git
```

## 协议 (Protocol)

OpenMV → ESP32 使用单向 UART VIS 协议:

```
VIS:cx,cy,w,h,feetY,conf,TYPE,distScore*checksum\r\n
```

- 所有坐标为 model 空间 (192×192)
- `TYPE` = `PERSON` (有人) 或 `NONE` (无人)
- `checksum` = 数据字段 (不含 `VIS:` 前缀和 `*`) 的 XOR
- 频率: 200ms 间隔
