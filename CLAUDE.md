# CLAUDE.md — 履带车视觉跟随系统 (Tracked Vehicle Visual Follower)

## Project Overview

金属履带车（载重 100kg）视觉跟随系统。三层架构: **OpenMV** 负责 YOLO person detection + VL53L1X ToF 激光测距 + 视觉距离融合; **ESP8266** 负责 FollowLogic 决策 + WiFi Dashboard; **STM32F103** 负责硬实时电机控制 + 安全保护。
OpenMV → ESP8266 通过 P0 HW UART(3) 4800 baud (VIS ASCII 协议), ESP8266 ↔ STM32 通过 UART0 swapped (GPIO15/GPIO13, 115200 baud, 6-byte MotorCmd 下行 + 遥测上行)。

## Hardware Platform

### 动力系统
- **电池**: 48V 89Ah 锂电池
- **电机**: 两台三相无刷电机 — 左右履带独立驱动
- **电调**: 两路独立三相无刷电调 (需双向正反转), 50Hz 舵机 PWM 接口 (1000-2000μs, 中位 1500μs)
- **控制方式**: STM32 坦克混控 (tank-mix): `left = throttle + (steering - 1500)`, `right = throttle - (steering - 1500)`
- **控制供电**: 48V → 5V 10A 防水降压模块 → ESP8266 + STM32 + OpenMV

### 控制板
| 板卡 | 角色 | 关键外设 |
|------|------|---------|
| OpenMV Cam N6 | L1 感知层 | YOLOv8n NPU (45FPS) + VL53L1X ToF (I2C2, P4/P5) + VIS P0 HW UART(3) |
| ESP8266 NodeMCU V3 (ESP-12E) | L2 决策层 | FollowLogic + WiFi Dashboard + VIS 接收 (D5/GPIO14) + STM32 (D7/D8) |
| STM32F103C8T6 (定制板) | L3 执行安全层 | PS2 (PB12-PB15) + USART3 ↔ ESP8266, TIM4 PWM → 双路无刷 ESC, 急停 GPIO |

> ESP32-WROOM-32U (DevKit V1) 已废弃。详见 `ESP32_Solo/DEPRECATED.md`。
> HC6060A 双向双路有刷电调已废弃 — 实车使用三相无刷电机。详见 `HC6060A_ESC_Technical_Reference.md` (§4.6 独立款混控算法仍可参考)。

### STM32 定制板详情
- **主控**: STM32F103C8T6 (Cortex-M3, 72MHz, 64KB Flash, 20KB SRAM)
- **USB-UART**: CH9102 via USB-C, 连 USART1 (PA9=TX, PA10=RX)
- **烧录**: PlatformIO serial @ 115200, 序列 `-dtr,rts,dtr,,,,` (DTR→NRST, RTS→BOOT0)
- **调试接口**: SWD (GND, PA14/SWCLK, 3V3, PA13/SWDIO)
- **PS2 手柄**: CN4 6P (PB12=CLK, PB13=CS, PB14=CMD, PB15=DATA), 5V 供电 (WHEELTEC 定义)
- **电源**: USB-C 供电, 板载 DC-DC 降压, 电源开关 + LED1(电源指示, 红色)
- **用户外设**:
  - LED2 (蓝色, PA4, active-LOW) — 心跳指示
  - BEEP 蜂鸣器 (PA3, 经跳线→S8050, active-HIGH) — 用户可编程
  - User 按键 + Reset 按键
  - PB10/PB11 底部端子引出 (USART3, 连 ESP8266)
- **其他 LED**: LED3/LED4 (红色) — 电源指示灯, 非 GPIO 控制

### 通信链路
- VL53L1X ToF → OpenMV N6: I2C(2) (SCL=P4, SDA=P5), addr 0x29, 40-4000mm, ±1mm, max 50Hz
- OpenMV N6 → ESP8266: P0 HW UART(3) (4800 baud) → D5 (GPIO14) SoftwareSerial, VIS ASCII 协议 (XOR checksum)
- ESP8266 → STM32: UART0 swapped (TX=D8/GPIO15, RX=D7/GPIO13) → STM32 USART3 (PB10/PB11), 115200
- ESP8266 → STM32 下行协议: 6-byte frame: `[0xAA] [th_lo] [th_hi] [st_lo] [st_hi] [CRC8]`
- STM32 → 左 ESC: TIM4 CH3 (PB8), 50Hz PWM, 1000-2000μs
- STM32 → 右 ESC: TIM4 CH4 (PB9), 50Hz PWM, 1000-2000μs
- **坦克混控**: STM32 将 throttle+steering 转换为左/右独立 PWM

### ESP8266 引脚分配
| 引脚 | 功能 | 设备 | 波特率 | 方向 |
|------|------|------|--------|------|
| D5 (GPIO14) | SoftwareSerial RX | OpenMV P0 HW UART(3) | 4800 | 单向 RX — VIS 帧 |
| D8 (GPIO15) | UART0 TX (swapped) | STM32 PB11 (USART3 RX) | 115200 | → MotorCmd 下行 |
| D7 (GPIO13) | UART0 RX (swapped) | STM32 PB10 (USART3 TX) | 115200 | ← 遥测上行 |
| GND | 共地 | STM32 GND | — | — |

> **`Serial.swap()`**: UART0 从 GPIO1/3 重映射到 GPIO15/13。原 USB-Serial (CH340) 不可用 — 调试通过 WiFi Dashboard。
> **GPIO15 启动**: NodeMCU 板载 10k 下拉, 正常启动。STM32 PB11 为输入态不驱动, 无冲突。

### STM32 引脚分配 (C06B 板)
| 引脚 | 功能 | 连接 |
|------|------|------|
| PA9/PA10 | USART1 (CH9102 USB-UART) | 烧录 + debug console |
| PB10/PB11 | USART3 | ← ESP8266 D7/D8 |
| PB8 (TIM4_CH3) | 左电机 ESC PWM | H6 舵机接口 (5V 组, 220Ω) |
| PB9 (TIM4_CH4) | 右电机 ESC PWM | H7 舵机接口 (5V 组, 220Ω) |
| PB12 | PS2 CLK (SCK) | CN4 pin 6 |
| PB13 | PS2 CS (ATTN) | CN4 pin 5 |
| PB14 | PS2 CMD (MOSI) | CN4 pin 4 |
| PB15 | PS2 DATA (MISO) | CN4 pin 3 |
| PA3 | BEEP 蜂鸣器 (经跳线, active-HIGH) | S8050 驱动 |
| PA4 | LED2 (active-LOW) | 心跳/模式指示 |

**控制优先级**: PS2 手柄 (接入时) > ESP8266 串口。PS2 START 键切换电机解锁/锁定。
PS2 左摇杆: Y(上下)=油门, X(左右)=转向。LED2 快闪(100ms)=PS2 模式, 慢闪(500ms)=ESP8266 模式。

### OpenMV N6 引脚分配
| 引脚 | 功能 | 说明 |
|------|------|------|
| P4 | I2C(2) SCL | VL53L1X ToF (独占) |
| P5 | I2C(2) SDA | VL53L1X ToF (独占) |
| P0 | SW UART TX | VIS → ESP8266 D5, 4800 baud |

## Language Convention

**注释和文档使用中文 + 英文专业名词。** 技术术语 (YOLO, PWM, UART, GPIO, CRC, ESC, ToF, VL53L1X, I2C, BLDC 等) 保持英文；解释性文字、架构说明、注意事项使用中文。

## Build System

- **ESP8266**: NodeMCU V3 (ESP-12E), Arduino IDE。入口: `ESP8266_Bridge/ESP8266_Bridge.ino`。基于 ESP8266 Arduino core 3.1.2。
- **STM32**: STM32F103C8T6 (定制板), PlatformIO (板: `genericSTM32F103CB`)。入口: `STM32_Solo/src/main.cpp`。
  - **Serial 注意**: generic 变体默认 `Serial`→USART2 (PA2/PA3)。定制板 CH9102 连接 USART1 (PA9/PA10)，需用 `HardwareSerial SerialUSART1(PA10, PA9)` 覆盖。
  - **Serial3 (ESP8266 通信)**: USART3 on PB10(TX)/PB11(RX), 115200 baud, 6-byte MotorCmd 下行帧。
  - **ESC PWM**: TIM4 CH3=PB8(左电机), CH4=PB9(右电机), 50Hz, 1000-2000μs。坦克混控由 MCU 完成。
- **OpenMV**: MicroPython，直接运行于 OpenMV Cam。无编译步骤 — 将 `.py` 文件复制到摄像头。
- **ESP32** (已废弃): `ESP32_Solo/` 保留作为参考实现，不再构建或部署。详见 `ESP32_Solo/DEPRECATED.md`。

## Architecture Conventions

- **三层架构**: L1 OpenMV (感知) → L2 ESP8266 (决策+WiFi) → L3 STM32 (执行+安全)。
- **安全优先**: 6 条独立关断路径 — 硬线急停、过流保护、命令超时、遥控失联 (Phase 2+)、视觉超时 (已实现)、控制器休眠 (已实现)。
- ESP8266 单线程主循环: VIS 接收 → FollowLogic → MotorCmd 下行 (50ms 间隔) + HTTP server。
- 无动态分配 — 固定大小 buffer 和栈分配 struct。
- `Config.h` 是引脚、阈值、参数的单一数据源 (single source of truth)。
- 电机控制: STM32 TIM4 CH3(PB8)→左电机, CH4(PB9)→右电机, 50Hz PWM。坦克混控将 throttle+steering 转换为左右独立 PWM。
- `MotorCmd` 为 `struct { uint16_t throttle; uint16_t steering; }`, 单位 μs (1000-2000, 中位 1500)。STM32 端做坦克混控转换为左/右 PWM。
- **坦克混控公式**: `left = throttle + (steering - 1500)`, `right = throttle - (steering - 1500)`, 钳位 1000-2000。

## Code Style

- C++ for ESP8266 & STM32 (`.cpp`/`.h`/`.ino`), MicroPython for OpenMV (`.py`)。
- 引脚分配为 `constexpr` in `Config.h`。
- `FollowLogic::update()` 返回 `MotorCmd` struct (throttle + steering in μs)。
- OpenMV 使用 assert 验证配置参数阈值。

## Safety Architecture

| # | 触发源 | 实现层 | 机制 | 状态 |
|---|--------|--------|------|------|
| 1 | 急停按钮 | STM32 GPIO + 继电器 | 物理断开 48V 动力电 | 未实现 |
| 2 | 过流检测 | STM32 ADC | 电流超阈值 → PWM 归零 | 未实现 |
| 3 | 命令超时 | STM32 SysTick | 500ms 无命令 → STOP (1500μs) | 已实现 |
| 4 | 遥控失联 | — | CRSF link lost → STOP | Phase 2+ |
| 5 | 视觉超时 | ESP8266 主循环 | VIS_TIMEOUT=700ms → 降级 STOP | 已实现 |
| 6 | 控制器休眠 | STM32 PS2 轮询 | 数据冻结 10s → 自动锁定 + 蜂鸣 | 已实现 |

## Testing

- ESP8266 FollowLogic tests in `Test/ESP32/FollowLogic_test.cpp` — PC 端 GCC 编译, 算法与 ESP8266 一致。
- OpenMV tests in `Test/OpenMV/` — PC 端 Python/pytest 运行, 使用纯函数副本避免硬件依赖。

## Available Agents & Skills

- `cortex-debugger`: STM32 firmware crash analysis (HardFault, stack overflow, interrupt conflicts)
- `protocol-analyzer`: UART protocol debugging (VIS frames, MotorCmd frames, checksum verification)

## Git LFS

仅 `*.tflite` 文件由 Git LFS 追踪。所有源代码为常规 Git objects。
