# CLAUDE.md — 履带车视觉跟随系统 (Tracked Vehicle Visual Follower)

## Project Overview

金属履带车（载重 100kg）视觉跟随系统。三层架构: **OpenMV** 负责 YOLO person detection + VL53L1X ToF 激光测距 + 视觉距离融合; **ESP32** 负责 FollowLogic 决策 + WiFi Dashboard (ESP8266 为下位备用硬件); **STM32F103** 负责硬实时电机控制 + 安全保护。
OpenMV → ESP32 (GPIO4) 或 ESP8266 (D5/GPIO14) 通过 P2 UART4 4800 baud (VIS ASCII 协议), ESP32 (Serial2, GPIO17/16) 或 ESP8266 (UART0 swapped, GPIO15/13) ↔ STM32 通过 115200 baud, 6-byte MotorCmd 下行 + 遥测上行。

## Hardware Platform

### 动力系统
- **电池**: 48V 89Ah 锂电池
- **电机**: 两台三相无刷电机 — 左右履带独立驱动
- **电调**: 两路独立三相无刷电调 (需双向正反转), 50Hz 舵机 PWM 接口 (650-1900μs, 中位 1275μs, ZTW Seal G2 非标)
- **控制方式**: STM32 坦克混控 (tank-mix): `left = throttle + (steering - 1275)`, `right = throttle - (steering - 1275)` (PWM_NEUTRAL=1275, ZTW Seal G2 非标中位)
- **控制供电**: 48V → 5V 10A 防水降压模块 → ESP32 + STM32 + OpenMV

### 控制板
| 板卡 | 角色 | 关键外设 |
|------|------|---------|
| OpenMV Cam N6 | L1 感知层 | YOLOv8n NPU (45FPS) + VL53L1X ToF (I2C2, P4/P5) + VIS P2 UART4 |
| ESP32-WROOM-32U (DevKit V1) | L2 决策层 (主) | FollowLogic + WiFi Dashboard + VIS 接收 (GPIO4) + STM32 (Serial2 GPIO17/16) + USB-Serial Debug |
| ESP8266 NodeMCU V3 (ESP-12E) | L2 决策层 (备用) | FollowLogic + WiFi Dashboard + VIS 接收 (D5/GPIO14) + STM32 (UART0 swapped D7/D8) |
| STM32F103C8T6 (定制板) | L3 执行安全层 | PS2 (PB12-PB15) + USART3 ↔ ESP32/ESP8266, TIM3 PWM → 双路无刷 ESC, 急停 GPIO |

> ESP32 为主控制器 (2026-06-03 重新启用); ESP8266 保留为下位备用硬件。两者 FollowLogic 算法完全相同，MotorCmd 协议一致。
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
  - PB10/PB11 底部端子引出 (USART3, 连 ESP32/ESP8266)
- **其他 LED**: LED3/LED4 (红色) — 电源指示灯, 非 GPIO 控制

### 通信链路
- VL53L1X ToF → OpenMV N6: I2C(2) (SCL=P4, SDA=P5), addr 0x29, 40-4000mm, ±1mm, max 50Hz
- OpenMV N6 →: P2 UART4 (4800 baud) → ESP32 GPIO4 (SoftwareSerial) 或 ESP8266 D5/GPIO14 (SoftwareSerial), VIS ASCII 协议 (XOR checksum)
- ESP32 → STM32: Serial2 (TX=GPIO17, RX=GPIO16) → STM32 USART3 (PB10/PB11), 115200
- ESP8266 → STM32 (备用): UART0 swapped (TX=D8/GPIO15, RX=D7/GPIO13) → STM32 USART3 (PB10/PB11), 115200
- 下行协议: 6-byte frame: `[0xAA] [th_lo] [th_hi] [st_lo] [st_hi] [CRC8]`
- STM32 → 左 ESC: TIM3 CH3 (PB0), 50Hz PWM, 650-1900μs (H8 接口)
- STM32 → 右 ESC: TIM3 CH4 (PB1), 50Hz PWM, 650-1900μs (H9 接口)
- **坦克混控**: STM32 将 throttle+steering 转换为左/右独立 PWM

### ESP32 引脚分配 (主控制器)
| 引脚 | 功能 | 设备 | 波特率 | 方向 |
|------|------|------|--------|------|
| GPIO4 | SoftwareSerial RX | OpenMV P2 UART4 | 4800 | 单向 RX — VIS 帧 |
| GPIO17 | Serial2 TX | STM32 PB11 (USART3 RX) | 115200 | → MotorCmd 下行 |
| GPIO16 | Serial2 RX | STM32 PB10 (USART3 TX) | 115200 | ← 遥测上行 |
| GPIO1/3 | UART0 USB-Serial | Debug console | 115200 | 调试输出 |
| GPIO2 | LED (active-HIGH) | 板载蓝色 LED | — | 运行指示 |
| GND | 共地 | STM32 GND | — | — |

> **烧录**: ESP32 stub loader 加载失败 (no sync reply)，需加 `--upload-property "upload.flags=--no-stub"` 使用 ROM bootloader。
> **WiFi 发射功率**: `esp_wifi_set_max_tx_power(80)` → 20 dBm (最大值)。

### ESP8266 引脚分配 (备用控制器)
| 引脚 | 功能 | 设备 | 波特率 | 方向 |
|------|------|------|--------|------|
| D5 (GPIO14) | SoftwareSerial RX | OpenMV P2 UART4 | 4800 | 单向 RX — VIS 帧 |
| D8 (GPIO15) | UART0 TX (swapped) | STM32 PB11 (USART3 RX) | 115200 | → MotorCmd 下行 |
| D7 (GPIO13) | UART0 RX (swapped) | STM32 PB10 (USART3 TX) | 115200 | ← 遥测上行 |
| GND | 共地 | STM32 GND | — | — |

> **`Serial.swap()`**: UART0 从 GPIO1/3 重映射到 GPIO15/13。原 USB-Serial (CH340) 不可用 — 调试通过 WiFi Dashboard。
> **GPIO15 启动**: NodeMCU 板载 10k 下拉, 正常启动。STM32 PB11 为输入态不驱动, 无冲突。

### STM32 引脚分配 (C06B 板)
| 引脚 | 功能 | 连接 |
|------|------|------|
| PA9/PA10 | USART1 (CH9102 USB-UART) | 烧录 + debug console |
| PB10/PB11 | USART3 | ← ESP32/ESP8266 |
| PB0 (TIM3_CH3) | 左电机 ESC PWM | H8 舵机接口 |
| PB1 (TIM3_CH4) | 右电机 ESC PWM | H9 舵机接口 |
| PB12 | PS2 CLK (SCK) | CN4 pin 6 |
| PB13 | PS2 CS (ATTN) | CN4 pin 5 |
| PB14 | PS2 CMD (MOSI) | CN4 pin 4 |
| PB15 | PS2 DATA (MISO) | CN4 pin 3 |
| PA3 | BEEP 蜂鸣器 (经跳线, active-HIGH) | S8050 驱动 |
| PA4 | LED2 (active-LOW) | 心跳/模式指示 |

**控制优先级**: PS2 手柄 (接入时) > ESP32/ESP8266 串口。PS2 START 键切换电机解锁/锁定。
PS2 左摇杆: Y(上下)=油门, X(左右)=转向。LED2 快闪(100ms)=PS2 模式, 慢闪(500ms)=ESP 模式。

### OpenMV N6 引脚分配
| 引脚 | 功能 | 说明 |
|------|------|------|
| P4 | I2C(2) SCL | VL53L1X ToF (独占) |
| P5 | I2C(2) SDA | VL53L1X ToF (独占) |
| P2 | UART4 TX | VIS → ESP32 GPIO4 / ESP8266 D5, 4800 baud |
| P0 | SPI2 MOSI | 无 UART 功能 (STM32N657) |

## Language Convention

**注释和文档使用中文 + 英文专业名词。** 技术术语 (YOLO, PWM, UART, GPIO, CRC, ESC, ToF, VL53L1X, I2C, BLDC 等) 保持英文；解释性文字、架构说明、注意事项使用中文。

## Build System

- **ESP32**: ESP32-WROOM-32U (DevKit V1), Arduino IDE。入口: `ESP32_Solo/ESP32_Solo.ino`。主控制器，功能完整 (VIS + FollowLogic + Dashboard + STM32 通信 + USB-Serial Debug)。
  - **烧录注意**: ESP32 的 stub loader 加载失败 (no sync reply)，需加 `--upload-property "upload.flags=--no-stub"` 使用 ROM bootloader 烧录。
- **ESP8266**: NodeMCU V3 (ESP-12E), Arduino IDE。入口: `ESP8266_Bridge/ESP8266_Bridge.ino`。基于 ESP8266 Arduino core 3.1.2。备用硬件，FollowLogic 算法与 ESP32 完全相同。
- **STM32**: STM32F103C8T6 (定制板), PlatformIO (板: `genericSTM32F103CB`)。入口: `STM32_Solo/src/main.cpp`。
  - **Serial 注意**: generic 变体默认 `Serial`→USART2 (PA2/PA3)。定制板 CH9102 连接 USART1 (PA9/PA10)，需用 `HardwareSerial SerialUSART1(PA10, PA9)` 覆盖。
  - **Serial3 (ESP32/ESP8266 通信)**: USART3 on PB10(TX)/PB11(RX), 115200 baud, 6-byte MotorCmd 下行帧。
  - **ESC PWM**: TIM3 CH3=PB0(左电机), CH4=PB1(右电机), 50Hz, 650-1900μs。坦克混控由 MCU 完成。
- **OpenMV**: MicroPython，直接运行于 OpenMV Cam。无编译步骤 — 将 `.py` 文件复制到摄像头。

## Architecture Conventions

- **三层架构**: L1 OpenMV (感知) → L2 ESP32 (决策+WiFi, 主) 或 ESP8266 (备用) → L3 STM32 (执行+安全)。
- **安全优先**: 6 条独立关断路径 — 硬线急停、过流保护、命令超时、遥控失联 (Phase 2+)、视觉超时 (已实现)、控制器休眠 (已实现)。
- ESP32/ESP8266 单线程主循环: VIS 接收 → FollowLogic → MotorCmd 下行 (50ms 间隔) + HTTP server。两者 FollowLogic 算法完全相同。
- 无动态分配 — 固定大小 buffer 和栈分配 struct。
- `Config.h` 是引脚、阈值、参数的单一数据源 (single source of truth)。
- 电机控制: STM32 TIM3 CH3(PB0)→左电机, CH4(PB1)→右电机, 50Hz PWM。坦克混控将 throttle+steering 转换为左右独立 PWM。
- `MotorCmd` 为 `struct { uint16_t throttle; uint16_t steering; }`, 单位 μs。STM32 端做坦克混控转换为左/右 PWM。
- **坦克混控公式**: `left = throttle + (steering - PWM_NEUTRAL)`, `right = throttle - (steering - PWM_NEUTRAL)`, 钳位 PWM_MIN~PWM_MAX。
- **摇杆映射**: PS2 摇杆满幅映射 PWM_MIN~PWM_MAX。LOCKED 时 PWM 锁定中位(安全), ARMED 后摇杆生效。

## Known Hardware Limitations

### ZTW Seal G2 电调 PWM 中位适配 (已验证)
- **核心发现**: ZTW Seal G2 船用电调的中位识别点不在标准 1500μs, 而在 **~1000-1275μs** 区间。设置 PWM_NEUTRAL=1275 后电调正常工作。FPV 接收机直接输出时脉宽波形与电调内置中位近似, 而 STM32 在 1500μs 时的脉宽信号被电调判定为前进而非停止。
- **当前参数**: `PWM_NEUTRAL=1275, PWM_MIN=650, PWM_MAX=1900` (经实测标定)。
- 同类案例: Flycolor 120A 船用电调也被报告以 1000-1100μs 为停止点。部分船用电调使用非标中位是已知现象。

### C06B 板 PWM 信号注意事项
- **H8/H9 接口串联 220Ω 电阻**: PB0/PB1 经 R34/R35 (220Ω) 连接到舵机接口排针, 限制驱动电流。如信号异常可短接电阻飞线。
- **GPIO 输出 3.3V 电平**: STM32F103 所有 GPIO 为 3.3V。标准 RC 接收机输出 5V PWM。

### 已知 Bug (已修复)
- **`escInit()` CRH 寄存器位偏移错误**: 配置了 PB12/PB13 而非 PB8/PB9, 导致 TIM4 PWM 无输出。修复后迁移至 TIM3 PB0/PB1。
- **LOCKED 状态 PWM 泄漏**: 曾去掉 `g_motorArmed` 条件以方便 ESC 校准, 导致 LOCKED 时摇杆仍能输出 PWM。已修复恢复安全检查。

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
| 3 | 命令超时 | STM32 SysTick | 20s 无命令 → 归中 (PWM_NEUTRAL) | 已实现 |
| 4 | 遥控失联 | — | CRSF link lost → STOP | Phase 2+ |
| 5 | 视觉超时 | ESP32/ESP8266 主循环 | VIS_TIMEOUT=700ms → 降级 STOP | 已实现 |
| 6 | 控制器休眠 | STM32 PS2 轮询 | 数据冻结 10s → 自动锁定 + 蜂鸣 | 已实现 |

## Testing

- ESP8266 FollowLogic tests in `Test/ESP32/FollowLogic_test.cpp` — PC 端 GCC 编译, 算法与 ESP8266 一致。
- OpenMV tests in `Test/OpenMV/` — PC 端 Python/pytest 运行, 使用纯函数副本避免硬件依赖。

## Available Agents & Skills

- `cortex-debugger`: STM32 firmware crash analysis (HardFault, stack overflow, interrupt conflicts)
- `protocol-analyzer`: UART protocol debugging (VIS frames, MotorCmd frames, checksum verification)

## Git LFS

仅 `*.tflite` 文件由 Git LFS 追踪。所有源代码为常规 Git objects。
