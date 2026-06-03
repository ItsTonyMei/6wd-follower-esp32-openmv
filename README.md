# 履带车视觉跟随系统 (Tracked Vehicle Visual Follower)

金属履带车（载重 100kg）视觉跟随系统。三层架构：OpenMV (感知) → ESP8266 (决策+WiFi) → STM32 (执行+安全)。ESP32 已废弃。

## 架构

```
OpenMV N6 (L1 感知)         ESP8266 (L2 决策+WiFi)        STM32F103 (L3 执行+安全)
├─ YOLOv8n NPU 45FPS        ├─ VIS 接收 D5@4800bd        ├─ PS2 手柄 (PB12-15)
├─ VL53L1X ToF (P4/P5)      ├─ FollowLogic 决策           ├─ USART3 ← ESP8266
└─ VIS P2 UART4@4800 →         ├─ UART0 swapped → STM32     ├─ 坦克混控 (tank-mix)
                             ├─ WiFi AP Dashboard          ├─ PB8=左电机, PB9=右电机
                             └─ /status JSON API           ├─ 双路三相无刷 ESC
                                                           ├─ LED2 + 蜂鸣器
                                                           └─ 命令超时 500ms
```

## 动力系统

- **电机**: 两台三相无刷电机 — 左右履带独立驱动
- **电调**: 两路独立三相无刷电调 (需双向正反转), 50Hz 舵机 PWM
- **控制**: STM32 坦克混控 `left = throttle + (steering - 1500)`, `right = throttle - (steering - 1500)`
- **电池**: 48V 89Ah 锂电池

## 目录结构

```
├── OpenMV/                    # N6 固件 (MicroPython)
│   └── main.py                # YOLOv8n + ToF + VIS P2 UART4 @ 4800
├── ESP8266_Bridge/            # ESP8266 全功能控制器 (Arduino)
│   ├── ESP8266_Bridge.ino     # VIS + FollowLogic + STM32 + Dashboard
│   ├── Config.h               # 引脚/阈值/参数
│   ├── FollowLogic.cpp/h      # 跟随决策逻辑
│   └── test_stm32_comm/       # STM32 通信测试
├── STM32_Solo/                # STM32 固件 (PlatformIO)
│   └── src/main.cpp           # PS2 + ESP8266 + 坦克混控 + ESC PWM + 安全
├── ESP32_Solo/                # ESP32 (已废弃, 参考)
├── HC6060A_ESC_Technical_Reference.md  # 有刷电调参考 (已废弃, §4.6 独立混控可参考)
├── ROADMAP.md                 # 开发路线图
└── CLAUDE.md                  # 项目手册
```

## 通信协议

```
N6 P2 UART4 @ 4800bd → ESP32 GPIO4 / ESP8266 D5 (GPIO14)
VIS:cx,cy,w,h,feetY,conf,PERSON,distScore,tofDist*XX\r\n

ESP8266 UART0 swapped (GPIO15/D8, GPIO13/D7) @ 115200bd → STM32 USART3 (PB11, PB10)
[0xAA] [throttle_lo] [throttle_hi] [steering_lo] [steering_hi] [CRC8]
6 bytes, STM32 收到后经坦克混控转换为左/右 PWM
```

## 硬件连接

| ESP8266 | STM32 C06B | 信号 |
|---------|-----------|------|
| D5 (GPIO14) | (OpenMV P0) | VIS 接收 |
| D8 (GPIO15) | PB11 (USART3 RX) | MotorCmd → |
| D7 (GPIO13) | PB10 (USART3 TX) | ← 遥测 |
| GND | GND | 共地 |

| STM32 | 左 ESC | 右 ESC | 说明 |
|-------|--------|--------|------|
| PB8 (TIM4_CH3) | 信号线 | — | 左电机 PWM (H6 接口) |
| PB9 (TIM4_CH4) | — | 信号线 | 右电机 PWM (H7 接口) |
| GND | GND | GND | 共地 |
| (不接) | BEC 5V | BEC 5V | MCU 独立供电 |

> 两路 ESC 的 BEC 5V 输出悬空不接 — MCU 由 48V→5V 降压模块独立供电。

## 快速开始

- **N6**: `main.py` 已写入闪存, 上电自启动
- **ESP8266**: 烧录 `ESP8266_Bridge/ESP8266_Bridge.ino`, WiFi AP "Tracked Robot"/12345678
- **STM32**: PlatformIO 打开 `STM32_Solo/`, 烧录; 按 PS2 START 解锁电机
- **Dashboard**: 手机连 WiFi 后访问 `http://192.168.4.1`

## Git LFS

仅 `*.tflite` 文件由 Git LFS 追踪。
