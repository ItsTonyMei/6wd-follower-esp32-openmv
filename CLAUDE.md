# CLAUDE.md — 履带车视觉跟随系统 (Tracked Vehicle Visual Follower)

## Project Overview

金属履带车（载重 100kg）视觉跟随系统。三层架构: **OpenMV** 负责 YOLO person detection + VL53L1X ToF 激光测距 + 视觉距离融合; **ESP32** 负责 FollowLogic 决策 + WiFi Dashboard + ELRS/CRSF 遥控接收; **STM32F103** 负责硬实时电机控制 + 安全保护。OpenMV → ESP32 通过 SoftSerial (VIS 协议), ESP32 ↔ STM32 通过 UART2 (MotorCmd + 遥测协议), ELRS 接收机 → ESP32 通过 UART1 (CRSF 协议 420k baud)。

## Hardware Platform

### 动力系统
- **电池**: 48V 89Ah 锂电池
- **电机电调**: 防水 48V 双向双路差速有刷电机电调 (2× 48V 45A 500W)
- **控制供电**: 48V → 5V 10A 防水降压模块 → ESP32 + STM32 + OpenMV

### 控制板
| 板卡 | 角色 | 关键外设 |
|------|------|---------|
| OpenMV Cam (H7+/N6) | L1 感知层 | Camera + YOLO LC + VL53L1X ToF 测距扩展板 (I2C) + UART3 TX |
| ESP32-WROOM-32U (DevKit V1) | L2 决策层 | ESP32-D0WD-V3 rev3.1, WiFi AP + ELRS/CRSF (UART1) + STM32 (UART2) |
| STM32F103C8T6 (定制板) | L3 执行安全层 | USART1 (PA9/PA10) ↔ ESP32, 2ch RC PWM → 电调, 急停 GPIO, ADC |

### STM32 定制板详情
- **主控**: STM32F103C8T6 (Cortex-M3, 72MHz, 64KB Flash, 20KB SRAM)
- **USB-UART**: CH9102 via USB-C, 连 USART1 (PA9=TX, PA10=RX)
- **烧录**: PlatformIO serial @ 115200, 序列 `-dtr,rts,dtr,,,,` (DTR→NRST, RTS→BOOT0)
- **调试接口**: SWD (GND, PA14/SWCLK, 3V3, PA13/SWDIO)
- **电源**: USB-C 供电, 板载 DC-DC 降压, 电源开关 + LED1(电源指示, 红色)
- **用户外设**:
  - LED2 (蓝色, PA4, active-LOW) — 用户可编程
  - BEEP 蜂鸣器 (PA3, active-LOW) — 用户可编程
  - User 按键 + Reset 按键
  - PB10/PB11 底部端子引出
- **其他 LED**: LED3/LED4 (红色) — 电源指示灯, 非 GPIO 控制

### 通信链路
- VL53L1X ToF → OpenMV: I2C (addr 0x29), 40-4000mm, ±1mm, max 50Hz
  - **N6**: I2C(2) 可用，Shield 接口或飞线 SCL→P4 SDA→P5
  - **H7 Plus**: I2C(2) (P4=SCL, P5=SDA), Shield 接口需确保接触良好，XSHUT 由 PCB 上拉至 VDD
- OpenMV → ESP32: EspSoftwareSerial GPIO18 RX, 115200, VIS ASCII 协议 (XOR checksum)
- ESP32 → STM32: UART2 TX=GPIO17, 115200, 4-byte 二进制 MotorCmd 协议 (CRC8)
- STM32 → ESP32: UART2 RX=GPIO16, 115200, 10-byte 遥测帧 (编码器/电流/状态)
- STM32 → 电调: 2ch 标准 RC PWM (1000-2000μs, 50Hz), 中位 1500μs=STOP
- ELRS RX → ESP32: UART1 RX=GPIO15 TX=GPIO4, CRSF 协议 420k baud, 16ch × 11-bit, 双向遥测

### ESP32 UART 分配
| UART | 引脚 | 设备 | 波特率 | 方向 |
|------|------|------|--------|------|
| UART0 | GPIO1/3 | USB Serial (CP2102) | 115200 | 双向 — debug/monitor |
| UART1 | RX=15, TX=4 | ELRS 接收机 (CRSF) | 420k | 双向 — RC通道+遥测 |
| UART2 | TX=17, RX=16 | STM32F103 | 115200 | 双向 — MotorCmd+遥测 |
| SoftSerial | RX=18 | OpenMV Cam | 115200 | 单向 RX — VIS 协议帧 |

## Language Convention

**注释和文档使用中文 + 英文专业名词。** 技术术语 (YOLO, PWM, FreeRTOS, UART, GPIO, CRSF, ELRS, CAN, CRC, ESC, BMS, ToF, VL53L1X, I2C, model, frame, packet 等) 保持英文；解释性文字、架构说明、注意事项使用中文。

## Build System

- **ESP32**: ESP32-WROOM-32U (DevKit V1), Arduino IDE 或 PlatformIO。入口: `ESP32_Solo/ESP32_Solo.ino`。基于 Arduino framework (底层 ESP-IDF)。
- **STM32**: STM32F103C8T6 (定制板, 非 Blue Pill), Arduino IDE (STM32duino) 或 PlatformIO (板: `genericSTM32F103CB`)。基于 STM32Duino core。
  - **Serial 注意**: generic 变体默认 `Serial`→USART2 (PA2/PA3)。定制板 CH9102 连接 USART1 (PA9/PA10)，需用 `HardwareSerial SerialUSART1(PA10, PA9)` 覆盖。
- **OpenMV**: MicroPython，直接运行于 OpenMV Cam。无编译步骤 — 将 `.py` 文件复制到摄像头。

## Architecture Conventions

- **三层架构**: L1 OpenMV (感知) → L2 ESP32 (决策) → L3 STM32 (执行+安全)。
- **安全优先**: 5 条独立关断路径 — 硬线急停、过流保护、命令超时、遥控失联、视觉超时。L3 的安全机制独立于 L1/L2。
- ESP32 上 FreeRTOS tasks: FollowLogic + VisionBridge on Core 0, WebTask (Dashboard) on Core 1。
- Task 间通信通过 FreeRTOS queue (`MotorCmd` struct, 2 bytes)。
- `DataAggregator` 封装互斥锁，统一管理 CarState + VisState + STM32 遥测数据。
- 主循环无动态分配 — 固定大小 queue 和栈分配 struct。
- `Config.h` 是引脚、阈值、task 参数的单一数据源 (single source of truth)。
- 电机控制: STM32 输出标准 RC PWM (1000-2000μs) 到电调，电调内部处理 H-bridge 换向和死区。
- Motor command 为 `enum class CarCmd : uint8_t`。

## Code Style

- C++ for ESP32 & STM32 (`.cpp`/`.h`), MicroPython for OpenMV (`.py`)。
- 引脚分配为 `constexpr` in `Config.h`。
- `FollowLogic::update()` 直接返回 `MotorCmd` struct，不再使用字符串协议。
- OpenMV 使用 assert 验证配置参数阈值。

## Safety Architecture

| # | 触发源 | 实现层 | 机制 |
|---|--------|--------|------|
| 1 | 急停按钮 | STM32 GPIO + 继电器 | 物理断开 48V 动力电 |
| 2 | 过流检测 | STM32 ADC | 电流超阈值 → PWM 归零 |
| 3 | 命令超时 | STM32 SysTick | 500ms 无命令 → STOP |
| 4 | 遥控失联 | ESP32 CRSF | ELRS link lost → STOP |
| 5 | 视觉超时 | ESP32 VisionBridge | VIS_TIMEOUT → 降级 STOP |

## Testing

- ESP32 unit tests in `Test/ESP32/` — PC 端 GCC 编译, `Test/ArduinoMock.h` 提供类型桩。
- OpenMV tests in `Test/OpenMV/` — PC 端 Python/pytest 运行, 使用纯函数副本避免硬件依赖。

## Available Agents & Skills

- `cortex-debugger`: ESP32/STM32 firmware crash analysis (Guru Meditation, HardFault, stack overflow, FreeRTOS deadlocks)
- `protocol-analyzer`: UART/CRSF protocol debugging (VIS frames, CRSF parsing, checksum verification)
- `esp32-firmware-engineer`: ESP-IDF specific guidance (build/flash/monitor, partition tables, sdkconfig, power optimization)

## Git LFS

仅 `*.tflite` 文件由 Git LFS 追踪。所有源代码为常规 Git objects。
