# 履带车视觉跟随系统 (Tracked Vehicle Visual Follower)

金属履带车（载重 100kg）视觉跟随系统。三层架构：OpenMV (感知) → ESP8266/ESP32 (决策) → STM32 (执行+安全)。

## 架构

```
N6 (L1 感知)           ESP8266 Bridge (L2 桥接)    ESP32 (L2 决策, 待恢复)
├─ YOLOv8n NPU 45FPS    ├─ VIS 接收 D5@4800bd      ├─ FollowLogic 决策
├─ VL53L1X ToF (P4/P5)  ├─ WiFi AP Dashboard       ├─ ELRS/CRSF 遥控
└─ VIS P0 SW UART@4800  └─ /status JSON API        └─ UART2 → STM32

                                                      STM32F103 (L3 安全)
                                                      ├─ 2ch RC PWM → 电调
                                                      ├─ 急停 + 过流保护
                                                      └─ 命令超时 500ms
```

## 目录结构

```
├── OpenMV/                    # N6 固件 (MicroPython)
│   ├── main.py                # YOLOv8n + ToF + VIS P0 SW UART @ 4800
│   └── test_vl53l1x.py        # VL53L1X ToF 功能验证
├── ESP8266_Bridge/            # ESP8266 桥接固件 (Arduino)
│   └── ESP8266_Bridge.ino     # VIS 接收 + WiFi Dashboard
├── ESP32_Solo/                # ESP32 决策固件 (Arduino, Phase 1+)
│   ├── ESP32_Solo.ino         # Clean Boot: WiFi AP + Dashboard
│   ├── Config.h               # 统一配置
│   ├── VisionBridge.cpp/h     # VIS 协议解析
│   ├── FollowLogic.cpp/h      # 跟随决策逻辑
│   ├── DataAggregator.cpp/h   # 数据聚合 + JSON
│   ├── DashboardServer.cpp/h  # HTTP Dashboard
│   └── index_html.h           # Dashboard 页面
├── STM32_Solo/                # STM32 固件 (PlatformIO)
│   └── src/main.cpp           # Blink 验证 (PA4 LED2)
├── ROADMAP.md                 # 开发路线图
├── CLAUDE.md                  # 项目手册
└── docs/                      # 文档
```

## Phase 0 验证结果 (2026-05-27)

| 板卡 | 状态 | 关键数据 |
|------|------|---------|
| N6 | ✅ | YOLOv8n 45FPS, VL53L1X I2C(2), VIS P0@4800 |
| ESP8266 | ✅ | VIS 接收 97%+ 成功率, WiFi Dashboard |
| ESP32 | ⚠️ | Serial2 GPIO16/17 异常, Phase 1+ 待换板 |
| STM32 | ✅ | PA4 LED2 验证, 工具链确认 |

## 通信协议

```
N6 P0 SW UART @ 4800bd → ESP8266 D5 (GPIO14)
VIS:cx,cy,w,h,feetY,conf,PERSON,distScore,tofDist*XX\r\n
```

## 快速开始

- **N6**: `main.py` 已写入闪存, 上电自启动
- **ESP8266**: 烧录 `ESP8266_Bridge/ESP8266_Bridge.ino`, WiFi AP "Tracked Robot"/12345678
- **Dashboard**: 手机连 WiFi 后访问 `http://192.168.4.1`
- **STM32**: PlatformIO 打开 `STM32_Solo/`, COM11 @ 115200 烧录

## Git LFS

仅 `*.tflite` 文件由 Git LFS 追踪。
