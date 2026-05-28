# ROADMAP — 履带车视觉跟随系统

## 当前进展 (2026-05-28)

### 已完成 & 已验证

| 模块 | 状态 | 关键成果 |
|------|------|---------|
| **STM32 + HC6060A 电调** | ✅ 已验证 | TIM4 CH3=PB8(黄线/转向), CH4=PB9(白线/油门), 50Hz PWM |
| **PS2 手柄控制** | ✅ 已验证 | 右摇杆上下=油门, 左摇杆左右=转向, START 解锁/锁定 |
| **蜂鸣器** | ✅ 已验证 | PA3 (经跳线→S8050, active-HIGH), ARM=两声短, DISARM=一声长 |
| **LED 指示** | ✅ 已验证 | PA4 快闪=ARMED, 中闪=LOCKED, 慢闪=无PS2 |
| **控制器休眠检测** | ✅ 已验证 | 数据冻结 10s → 自动锁定 + 蜂鸣 |
| **安全锁定** | ✅ 已验证 | 上电默认 LOCKED, 500ms 命令超时, PS2 断连自动归中 |
| **ESP8266 → STM32 协议** | ✅ 已定义 | 6-byte: `[0xAA][th_lo][th_hi][st_lo][st_hi][CRC8]` |
| **ESP8266 FollowLogic** | ✅ 代码就绪 | 移植自 ESP32, 连续 throttle+steering 输出 |
| **ESP8266 VIS 接收** | ✅ 已验证 | D5(GPIO14) SoftwareSerial @ 4800 ← N6, 97%+ 成功率 |
| **ESP8266 WiFi Dashboard** | ✅ 已验证 | AP "Rover", HTTP :80, `/status` JSON |
| **OpenMV N6** | ✅ 已验证 | YOLOv8n NPU 45FPS, VL53L1X ToF, VIS P0@4800 |
| **ESP32** | ❌ 已废弃 | ESP8266 替代全部功能, 详见 `ESP32_Solo/DEPRECATED.md` |

### 待完成

| 优先级 | 任务 | 说明 |
|--------|------|------|
| 🔴 高 | ESP8266 ↔ STM32 串口打通 | ESP8266 代码已就绪, STM32 端需加回 USART3 接收 |
| 🔴 高 | 电调实车接线测试 | PB8(黄) + PB9(白) → HC6060A, 双电机必须都接 |
| 🟡 中 | 遥控器 (ELRS/CRSF) | Phase 2, 备用遥控链路 |
| 🟡 中 | 自动跟随联调 | ESP8266 FollowLogic + OpenMV VIS → STM32 → 电机 |
| 🟢 低 | Dashboard 完善 | 加入 car 状态显示 |

---

## 架构总览

```
┌──────────────────────────────────────────────────────────────┐
│                     L1: 感知层 (OpenMV N6)                    │
│   YOLO person detection + VL53L1X ToF → VIS/UART → ESP8266   │
├──────────────────────────────────────────────────────────────┤
│                     L2: 决策层 (ESP8266)                      │
│   FollowLogic + WiFi Dashboard + VIS 接收                    │
│   接收: OpenMV VIS帧                                          │
│   输出: MotorCmd → UART → STM32                              │
│   (ESP32 已废弃, ESP8266 替代全部 L2 功能)                     │
├──────────────────────────────────────────────────────────────┤
│                   L3: 执行与安全层 (STM32)                     │
│   PS2 手柄 + ESP8266 串口 → PWM (HC6060A 混控电调)            │
│   命令超时 500ms + 控制器休眠 10s + 断连自动归中               │
└──────────────────────────────────────────────────────────────┘
```

## 安全链路设计

| # | 触发源 | 机制 | 响应时间 | 状态 |
|---|--------|------|---------|------|
| 1 | 急停按钮 | 继电器物理断开电机电源 | <10ms | 未实现 |
| 2 | 过流检测 | ADC阈值 → PWM归零 | <1ms | 未实现 |
| 3 | 命令超时 | 500ms无有效命令 → STOP (1500μs) | <500ms | **已实现** |
| 4 | 遥控失联 | CRSF link lost → STOP | <100ms | Phase 2+ |
| 5 | 视觉超时 | VIS_TIMEOUT=700ms → 降级 STOP | <700ms | **已实现** |
| 6 | 控制器休眠 | PS2 数据冻结 10s → 自动锁定 + 蜂鸣 | <10s | **已实现** |

## 数据流

```
OpenMV N6                     ESP8266                        STM32
─────────                     ───────                        ──────
YOLO检测                       VIS接收 (SoftwareSerial)       PS2 轮询 (50Hz)
  │                              │                             │
  ├── VL53L1X ToF (I2C)          ▼                             ├── ESP8266 串口
  │     └→ 距离(mm)             FollowLogic.update()            │    (USART3, 待接入)
  ▼                              │                             ▼
视觉+ToF融合距离                  ▼                            MotorCmd 解析
  │                             MotorCmd {thr, st} μs           │
  ▼                              │                             ▼
VIS帧组装                        ▼                            TIM4 PWM 输出
  │                             UART0 swapped → STM32          │   PB8=黄线(转向)
P0 SW UART@4800 → ESP8266      6-byte binary + CRC8            │   PB9=白线(油门)
                                                               ▼
                                                             安全监控
                                                               · 500ms 超时
                                                               · 10s 休眠检测
                                                               · PS2 断连保护
```

---

## Phase 0 — 项目初始化与硬件验证 ✅ 核心完成

**状态**: STM32 ✅ | OpenMV N6 ✅ | ESP8266 ✅ | ESP32 ❌ 已废弃

> **通信方案**: N6 P0 SW UART @ 4800bd → ESP8266 D5 SoftwareSerial → WiFi Dashboard
> **ESP8266 已成为永久 L2 控制器** (替代 ESP32)。ESP32_Solo/ 保留作为参考实现。
> **待推进**: 电调实车接线, ESP8266↔STM32 串口打通, 自动跟随联调

### 0.1 STM32F103C8T6 开发环境 ✅ 完成
- [x] PlatformIO + STM32Duino, USB-TTL 烧录 (CH9102, COM11 @ 115200)
- [x] Blink 验证 (PA4 LED2)
- [x] **HC6060A ESC PWM 验证**: TIM4 CH3=PB8(黄线/转向), CH4=PB9(白线/油门), 50Hz
- [x] **PS2 手柄驱动**: WHEELTEC 引脚 (PB12=CLK, PB13=CS, PB14=CMD, PB15=DATA)
- [x] **蜂鸣器**: PA3 (经跳线→S8050, active-HIGH)
- [x] **安全功能**: 上电锁定, START 解锁, 500ms 超时, 10s 休眠检测
- [x] **代码精简**: 441→290 行, RAM 7.1%, Flash 11.3%

### 0.2 ESP8266 全功能控制器 ✅ 完成
- [x] VIS 接收: D5(GPIO14) SoftwareSerial @ 4800, 97%+ 成功率
- [x] FollowLogic 移植: 连续 throttle+steering 输出 (与 ESP32 算法一致)
- [x] WiFi AP "Rover" + HTTP Dashboard (:80)
- [x] STM32 通信: UART0 swapped (D8=TX→PB11, D7=RX←PB10), 115200
- [x] MotorCmd 下行协议: `[0xAA][th_lo][th_hi][st_lo][st_hi][CRC8]` 6-byte
- [x] Config.h: 引脚/阈值/参数 SSOT

### 0.3 OpenMV N6 ✅ 完成
- [x] YOLOv8n NPU @ 45FPS, VL53L1X ToF (I2C2: P4/P5, addr 0x29)
- [x] VIS P0 SW UART @ 4800 → ESP8266 D5
- [x] 帧格式: `VIS:cx,cy,w,h,feetY,conf,PERSON,distScore,tofDist*CS\r\n`

### 0.4 硬件清单

| 硬件 | 状态 | 用途 |
|------|------|------|
| STM32F103C8T6 (C06B 定制板) | **已有** | L3 执行+安全 |
| ESP8266 NodeMCU V3 | **已有** | L2 决策+WiFi |
| OpenMV Cam N6 + VL53L1X | **已有** | L1 感知 |
| HC6060A 混控电调 | **已到货** | 电机驱动 |
| 48V→5V 10A 降压模块 | **已到货** | 控制供电 |
| 48V 89Ah 锂电池 | **随车已有** | 动力电源 |
| PS2 无线手柄 | **已有** | 遥控测试 |
| ~~ESP32-WROOM-32U~~ | **已废弃** | 被 ESP8266 替代 |
| ELRS 遥控器 + 接收机 | ⏳ 待采购 | Phase 2 |
| 急停按钮 + 继电器 | ⏳ 待采购 | 安全链路 #1 |

---

## Phase 1 — STM32 底层驱动 (PS2 遥控部分已完成 ✅, ESP8266 串口待接入)

### 1.1 ESC PWM 控制 ✅
- [x] TIM4 CH3/CH4 50Hz, 1000-2000μs, 中位 1500μs
- [x] HC6060A 混控款: PB8=黄线(转向), PB9=白线(油门)
- [ ] 电调实车接线测试 (双电机必须都接好)
- [ ] ESC 上电自检流程验证

### 1.2 PS2 手柄 ✅
- [x] WHEELTEC 引脚映射 + bit-bang SPI 驱动
- [x] 右摇杆上下=油门, 左摇杆左右=转向
- [x] START 解锁/锁定, 蜂鸣提示
- [x] 控制器休眠 10s 自动锁定

### 1.3 ESP8266 串口通信 (协议已定义, STM32 端待接入)
ESP8266 → STM32 下行 (每 50ms):
```
[0xAA] [throttle_lo] [throttle_hi] [steering_lo] [steering_hi] [CRC8]
6 bytes, CRC8 over bytes 1-4, poly 0x07
```
- [ ] STM32 USART3 (PB10/PB11) 接收 + CRC8 校验
- [ ] PS2 优先, ESP8266 备选

### 1.4 安全机制
- [x] 命令超时 500ms → STOP
- [x] PS2 断连 → 自动锁定 + 归中
- [ ] 急停按钮 GPIO + 继电器 (硬件待采购)
- [ ] 过流检测 ADC (硬件待确认)

---

## Phase 2 — 遥控器集成 (ELRS/CRSF, 待采购)

**目标**: ELRS 遥控链路打通, Manual 模式作为 PS2 的升级替代。

| 设备 | 推荐 |
|------|------|
| 遥控器 | Jumper T14 ELRS (EdgeTX, ELRS 2.4G 1W) |
| 接收机 | Radiomaster ER6 / Happymodel EP1 (CRSF 输出) |

> 接收机通过 UART 输出 CRSF 协议帧到 ESP8266。通道映射: CH1=转向, CH3=油门, CH5=模式, CH6=限速。
> CRSF 协议自带 RSSI + LQ 链路质量, 失联自动 FailSafe。
> Phase 2+ 接入 ESP8266 的 UART1 或者 SoftwareSerial。

---

## Phase 3 — ESP8266 ↔ STM32 UART 通信协议 (协议已定, 待联调)

**下行 (ESP8266 → STM32, 每 50ms)**:
```
[0xAA] [throttle_lo] [throttle_hi] [steering_lo] [steering_hi] [CRC8]
6 bytes, uint16_t throttle/steering in μs (1000-2000, 中位 1500)
```

**上行 (STM32 → ESP8266, 预留)**:
```
[0xBB] [EncL:2B] [EncR:2B] [CurL:1B] [CurR:1B] [Flags:1B] [ErrCode:1B] [CRC8:1B]
10 bytes
```

- [ ] ESP8266 端 UART0 swapped 发送 (代码已就绪)
- [ ] STM32 端 USART3 (PB10/PB11) 接收 (待接入)
- [ ] 心跳 + 超时处理

---

## Phase 4 — 自动跟随决策层 (FollowLogic 已就绪, 待联调)

- [x] ESP8266 FollowLogic: 连续 throttle+steering 输出
- [x] distScore 映射: FAR→全速, MID→中速, NEAR→慢速/STOP
- [x] cx 偏移 → steering 比例控制
- [x] 近距离转向抑制
- [ ] 端到端链路: OpenMV VIS → ESP8266 FollowLogic → STM32 PWM → 电机
- [ ] 实车跟随测试 + PID 整定

---

## Phase 5 — Dashboard 与远程监控 (基础版完成 ✅)

- [x] WiFi AP + HTTP Dashboard
- [x] VIS 数据实时显示 (置信度/距离分/ToF/检测框)
- [ ] Car 状态显示 (throttle/steering)
- [ ] 在线参数调整
- [ ] 日志记录

---

## Phase 6 — OpenMV 视觉校准 (基础验证完成 ✅)

- [x] YOLOv8n person detection + VL53L1X ToF 确认在线
- [x] VIS 帧格式: 含 tofDistance + XOR checksum
- [ ] 履带车低视角场景检测率测试
- [ ] distScore 阈值实车标定
- [ ] ToF 有效范围与视觉重叠区间确认

---

## Phase 7 — 全系统联调

- [ ] 上电启动顺序: STM32 → ESP8266 → OpenMV
- [ ] 端到端链路: 人→OpenMV→ESP8266→STM32→电调→电机
- [ ] 全链路延迟测试
- [ ] 安全机制全测试

---

## Phase 8 — 场地测试

- [ ] 室内平地跟随
- [ ] 户外/坡道测试
- [ ] 紧急场景测试 (目标消失/侧面闯入/堵转)
- [ ] 续航与散热

---

## 里程碑

| 里程碑 | 状态 | 标志性成果 |
|--------|------|-----------|
| M1: PS2 遥控 | ✅ | PS2 手柄控制履带车前进/后退/转向 |
| M2: 安全层基本 | ✅ | 命令超时 + 休眠检测 + 断连保护 |
| M3: ESP8266 控制器 | ✅ | VIS + FollowLogic + Dashboard 就绪 |
| M4: 电调实车 | ⏳ | HC6060A 接线 + 双电机驱动验证 |
| M5: 自动跟随 | ⏳ | 人走车走, 人停车停 |
| M6: 可以出门 | ⏳ | 户外各种地面稳定跟随 |
