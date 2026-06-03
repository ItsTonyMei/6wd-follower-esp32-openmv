# ROADMAP — 履带车视觉跟随系统

## 当前进展 (2026-06-02)

### 已完成 & 已验证

| 模块 | 状态 | 关键成果 |
|------|------|---------|
| **STM32 + PWM 输出** | ✅ 已验证 | TIM3 CH3=PB0(左电机 H8), CH4=PB1(右电机 H9), 50Hz PWM, 坦克混控 |
| **PS2 手柄控制** | ✅ 已验证 | 右摇杆上下=油门, 左摇杆左右=转向, START 解锁/锁定 |
| **蜂鸣器** | ✅ 已验证 | PA3 (经跳线→S8050, active-HIGH), ARM=两声短, DISARM=一声长 |
| **LED 指示** | ✅ 已验证 | PA4 快闪=ARMED, 中闪=LOCKED, 慢闪=无PS2 |
| **控制器休眠检测** | ✅ 已验证 | 数据冻结 30s → 自动锁定 + 蜂鸣 |
| **安全锁定** | ✅ 已验证 | 上电默认 LOCKED, 摇杆无操作 60s 自动锁定, PS2 断连立即锁定 |
| **ESP8266 → STM32 协议** | ✅ 已验证 | 6-byte CRC8 帧, USART3 115200, 50ms间隔, STM32端已打通 |
| **ESP8266 FollowLogic** | ✅ 代码就绪 | 移植自 ESP32, 连续 throttle+steering 输出 |
| **ESP8266 VIS 接收** | ✅ 已验证 | D5(GPIO14) SoftwareSerial @ 4800 ← N6, 97%+ 成功率 |
| **ESP8266 WiFi Dashboard** | ✅ 已验证 | AP "Tracked Robot", HTTP :80, `/status` JSON |
| **OpenMV N6** | ✅ 已验证 | YOLOv8n NPU 45FPS, VL53L1X ToF (I2C2 P4/P5), VIS P2 UART4 @ 4800 (STM32N657) |
| **ESP32** | ✅ 重新启用 | 精简固件就绪 (2026-06-03)。4文件单线程架构，功能匹配 ESP8266。烧录需 `--no-stub`。GPIO2 LED 正常。详见 `ESP32_Solo/DEPRECATED.md` |
| **HC6060A 有刷电调** | ❌ 已废弃 | 实车使用三相无刷电机, 替换为双路独立无刷电调 |
| **ZTW Seal G2 无刷电调** | ✅ 已解决 | 核心发现: 该电调中位在~1275μs而非1500μs。STM32在1500μs时被电调判定为前进, FPV接收机因脉宽波形差异恰好匹配。修改PWM_NEUTRAL=1275后正常 |

### 待完成

| 优先级 | 任务 | 说明 |
|--------|------|------|
| 🔴 高 | 坦克混控实车验证 | PB0(左)/PB1(右) → 双路 ESC → 电机实地测试 |
| 🟡 中 | 遥控器 (ELRS/CRSF) | Phase 2, 届时换 ESP32 (需3路硬件UART) |
| 🟡 中 | 自动跟随联调 | ESP8266 FollowLogic + OpenMV VIS → STM32 坦克混控 → 电机 |
| 🟢 低 | Dashboard 完善 | 加入 car 状态显示 |

---

## 硬件平台变更记录

| 日期 | 变更 | 原因 |
|------|------|------|
| 2026-06-02 | **ZTW Seal G2 中位适配解决** | 实测发现该电调中位识别点在~1275μs而非标准1500μs。FPV接收机与STM32脉宽波形差异导致1500μs时电调判定为前进。PWM_NEUTRAL→1275μs后正常 |
| 2026-06-02 | **PWM 引脚迁移 PB8/PB9→PB0/PB1 (TIM4→TIM3)** | PB8/PB9 硬件损坏, 迁移至 H8/H9; CRH→CRL; TIM4→TIM3 |
| 2026-06-02 | **STM32 PWM 输出修复** | `escInit()` CRH 寄存器位偏移错误修正 (PB12/13→PB8/9); 摇杆满幅映射 |
| 2026-06-01 | **ESP8266 ↔ STM32 USART3 通信打通** | 恢复 STM32 SerialESP 接收, CRC8 校验, PS2/ESP 通用超时 |
| 2026-06-01 | **N6 VIS: bit-bang → P0 硬件 UART(3)** | P0=USART3 TX, 省 CPU 资源, 更可靠 |
| 2026-06-03 | **N6 VIS: P0→P2 (UART3→UART4)** | STM32N657 官方引脚表: P0 无 UART, P2=UART4 TX |
| 2026-05-28 | ESP32 → ESP8266 | NodeMCU V3 替代 ESP32 全部 L2 功能 |

> **当前控制方案**: STM32 TIM3 输出两路 50Hz PWM (PB0=左电机 H8, PB1=右电机 H9), 坦克混控。PWM: 650-1275-1900μs。
> **混控公式**: `left = throttle + (steering - PWM_NEUTRAL)`, `right = throttle - (steering - PWM_NEUTRAL)`。

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
│   输出: MotorCmd {throttle, steering} → UART → STM32          │
├──────────────────────────────────────────────────────────────┤
│                   L3: 执行与安全层 (STM32)                     │
│   PS2 手柄 + ESP8266 串口                                     │
│   坦克混控: throttle+steering → 左/右独立 PWM                  │
│   输出: PB0(左电机) + PB1(右电机) → 双路三相无刷 ESC           │
│   无操作 60s 锁定 + 休眠 30s 锁定 + 断连立即锁定                │
└──────────────────────────────────────────────────────────────┘
```

## 安全链路设计

| # | 触发源 | 机制 | 响应时间 | 状态 |
|---|--------|------|---------|------|
| 1 | 急停按钮 | 继电器物理断开电机电源 | <10ms | 未实现 |
| 2 | 过流检测 | ADC阈值 → PWM归零 | <1ms | 未实现 |
| 3 | 命令超时 | 摇杆无操作 60s → 自动锁定 + 蜂鸣 | <60s | **已实现** |
| 4 | 遥控失联 | CRSF link lost → STOP | <100ms | Phase 2+ |
| 5 | 视觉超时 | VIS_TIMEOUT=700ms → 降级 STOP | <700ms | **已实现** |
| 6 | 控制器休眠 | PS2 数据冻结 30s → 自动锁定 + 蜂鸣 | <30s | **已实现** |

## 数据流

```
OpenMV N6                     ESP8266                        STM32
─────────                     ───────                        ──────
YOLO检测                       VIS接收 (SoftwareSerial)       PS2 轮询 (50Hz)
  │                              │                             │
  ├── VL53L1X ToF (I2C)          ▼                             ├── ESP8266 串口
  │     └→ 距离(mm)             FollowLogic.update()            │    (USART3, 已打通)
  ▼                              │                             ▼
视觉+ToF融合距离                  ▼                            MotorCmd 解析
  │                             MotorCmd {thr, st} μs           │
  ▼                              │                             ▼
VIS帧组装                        ▼                            坦克混控 (tank-mix)
  │                             UART0 swapped → STM32           │   left = thr + (st-PWM_NEUTRAL)
P2 UART4@4800 → ESP32/ESP8266      6-byte binary + CRC8             │   right = thr - (st-PWM_NEUTRAL)
                                                                 ▼
                                                               TIM3 PWM 输出
                                                                 │   PB0=左电机 ESC
                                                                 │   PB1=右电机 ESC
                                                                 ▼
                                                              安全监控
                                                                · 60s 无操作锁定
                                                                · 30s 休眠锁定
                                                                · PS2 断连锁定
```

---

## Phase 0 — 项目初始化与硬件验证 ✅ 核心完成

**状态**: STM32 ✅ | OpenMV N6 ✅ | ESP8266 ✅ | ESP32 ❌ 已废弃 | HC6060A ❌ 已废弃

> **动力方案变更**: 实车为两台三相无刷电机，需要双路独立无刷电调 + MCU 坦克混控。
> **ESP8266 已成为永久 L2 控制器** (替代 ESP32)。ESP32_Solo/ 保留作为参考实现。
> **待推进**: 坦克混控实车验证, 自动跟随联调

### 0.1 STM32F103C8T6 开发环境 ✅ 完成
- [x] PlatformIO + STM32Duino, USB-TTL 烧录 (CH9102, COM11 @ 115200)
- [x] Blink 验证 (PA4 LED2)
- [x] **PWM 输出验证**: TIM3 CH3=PB0(左电机 H8), CH4=PB1(右电机 H9), 50Hz, 坦克混控
- [x] **PS2 手柄驱动**: WHEELTEC 引脚 (PB12=CLK, PB13=CS, PB14=CMD, PB15=DATA)
- [x] **蜂鸣器**: PA3 (经跳线→S8050, active-HIGH)
- [x] **安全功能**: 上电锁定, START 解锁, 无操作 60s 锁定, 休眠 30s 锁定
- [x] **代码重构**: 参数集中管理, RAM 13.5%, Flash 15.3%

### 0.2 ESP8266 全功能控制器 ✅ 完成
- [x] VIS 接收: D5(GPIO14) SoftwareSerial @ 4800, 97%+ 成功率
- [x] FollowLogic 移植: 连续 throttle+steering 输出 (与 ESP32 算法一致)
- [x] WiFi AP "Rover" + HTTP Dashboard (:80)
- [x] STM32 通信: UART0 swapped (D8=TX→PB11, D7=RX←PB10), 115200
- [x] MotorCmd 下行协议: `[0xAA][th_lo][th_hi][st_lo][st_hi][CRC8]` 6-byte
- [x] Config.h: 引脚/阈值/参数 SSOT

### 0.3 OpenMV N6 ✅ 完成
- [x] YOLOv8n NPU @ 45FPS, VL53L1X ToF (I2C2: P4/P5, addr 0x29)
- [x] VIS P0 硬件 UART(3) @ 4800 → ESP8266 D5 (替代 bit-bang 软件串口)
- [x] 帧格式: `VIS:cx,cy,w,h,feetY,conf,PERSON,distScore,tofDist*CS\r\n`

### 0.4 硬件清单

| 硬件 | 状态 | 用途 |
|------|------|------|
| STM32F103C8T6 (C06B 定制板) | **已有** | L3 执行+安全 |
| ESP8266 NodeMCU V3 | **已有** | L2 决策+WiFi |
| OpenMV Cam N6 + VL53L1X | **已有** | L1 感知 |
| 三相无刷电机 ×2 | **随车已有** | 左右履带独立驱动 |
| ZTW Seal G2 无刷电调 ×2 | **已有** | 独立控制左右电机 (PWM_NEUTRAL=1275μs) |
| 48V→5V 10A 降压模块 | **已到货** | 控制供电 |
| 48V 89Ah 锂电池 | **随车已有** | 动力电源 |
| PS2 无线手柄 | **已有** | 遥控测试 |
| ~~HC6060A 有刷电调~~ | **已废弃** | 不兼容三相无刷电机 |
| ~~ESP32-WROOM-32U~~ | **已废弃** | 被 ESP8266 替代 |
| ELRS 遥控器 + 接收机 | ⏳ 待采购 | Phase 2 |
| 急停按钮 + 继电器 | ⏳ 待采购 | 安全链路 #1 |

---

## Phase 1 — STM32 底层驱动 (PS2 遥控部分已完成 ✅, 无刷电调待验证)

### 1.1 ESC PWM 控制 ✅ 已完成
- [x] TIM3 CH3=PB0(左 H8), CH4=PB1(右 H9), 50Hz PWM, 坦克混控
- [x] ZTW Seal G2 电调已采购 + 中位标定 (PWM_NEUTRAL=1275, MIN=650, MAX=1900)
- [x] 坦克混控: `left = thr + (st-PWM_NEUTRAL)`, `right = thr - (st-PWM_NEUTRAL)`
- [ ] 电调接线 + 坦克混控实车测试 (PB0/PB1→H8/H9→ESC)

### 1.2 PS2 手柄 ✅
- [x] WHEELTEC 引脚映射 + bit-bang SPI 驱动
- [x] 右摇杆上下=油门, 左摇杆左右=转向
- [x] START 解锁/锁定, 蜂鸣提示
- [x] 控制器休眠 30s 自动锁定
- [x] SELECT 切换 PS2/ESP8266 控制源

### 1.3 ESP8266 串口通信 ✅ 已打通
ESP8266 → STM32 下行 (每 50ms):
```
[0xAA] [throttle_lo] [throttle_hi] [steering_lo] [steering_hi] [CRC8]
6 bytes, CRC8 over bytes 1-4, poly 0x07
```
- [x] STM32 USART3 (PB10/PB11) 接收 + CRC8 校验 (SerialESP)
- [x] PS2 优先 (SELECT 切换), ESP8266 备选
- [x] ESP 模式下 MotorCmd → 坦克混控 → 左右 PWM
- [x] 命令超时 PS2/ESP 通用 (无操作 60s → 自动锁定+蜂鸣)

### 1.4 安全机制
- [x] 摇杆无操作 60s → 自动锁定 + 蜂鸣
- [x] PS2 数据冻结 30s → 休眠锁定 + 蜂鸣
- [x] PS2 断连 → 立即锁定 + 归中
- [ ] 急停按钮 GPIO + 继电器 (硬件待采购)
- [ ] 过流检测 ADC (硬件待确认)

---

## Phase 2 — 遥控器集成 (ELRS/CRSF, 待采购)

**目标**: ELRS 遥控链路打通, Manual 模式作为 PS2 的升级替代。

| 设备 | 推荐 |
|------|------|
| 遥控器 | Jumper T14 ELRS (EdgeTX, ELRS 2.4G 1W) |
| 接收机 | Radiomaster ER6 / Happymodel EP1 (CRSF 输出) |

> 接收机通过 UART 输出 CRSF 协议帧。通道映射: CH1=转向, CH3=油门, CH5=模式, CH6=限速。
> CRSF 协议自带 RSSI + LQ 链路质量, 失联自动 FailSafe。
> ESP8266 硬件串口已用满 (UART0→STM32, UART1仅TX无RX)。Phase 2 换回 ESP32 (3路全双工 UART)。

---

## Phase 3 — ESP8266 ↔ STM32 UART 通信 ✅ 已打通

**下行 (ESP8266 → STM32, 每 50ms)**:
```
[0xAA] [throttle_lo] [throttle_hi] [steering_lo] [steering_hi] [CRC8]
6 bytes, uint16_t throttle/steering in μs (650-1900, 中位 1275)
```
STM32 收到后经坦克混控转换为左/右 PWM。

**上行 (STM32 → ESP8266, 预留)**:
```
[0xBB] [EncL:2B] [EncR:2B] [CurL:1B] [CurR:1B] [Flags:1B] [ErrCode:1B] [CRC8:1B]
10 bytes
```

- [x] ESP8266 端 UART0 swapped 发送 (代码就绪)
- [x] STM32 端 USART3 (PB10/PB11) 接收 + CRC8 校验 (已实现)
- [x] 无操作 60s 自动锁定 (基于摇杆值变化检测)
- [x] PS2 SELECT 键切换控制源

---

## Phase 4 — 自动跟随决策层 (FollowLogic 已就绪, 待联调)

- [x] ESP8266 FollowLogic: 连续 throttle+steering 输出
- [x] distScore 映射: FAR→全速, MID→中速, NEAR→慢速/STOP
- [x] cx 偏移 → steering 比例控制
- [x] 近距离转向抑制
- [ ] 端到端链路: OpenMV VIS → ESP8266 FollowLogic → STM32 坦克混控 → 双路 ESC → 电机
- [ ] 实车跟随测试 + 坦克混控参数整定

---

## Phase 5 — Dashboard 与远程监控 (基础版完成 ✅)

- [x] WiFi AP + HTTP Dashboard
- [x] VIS 数据实时显示 (置信度/距离分/ToF/检测框)
- [x] Car 状态显示 (throttle/steering/L/R PWM, 串口 5Hz + OLED)
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
- [ ] 端到端链路: 人→OpenMV→ESP8266→STM32→坦克混控→双路ESC→电机
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
| M4: 无刷电调驱动 | ⏳ | 坦克混控 + 双路 ESC + 电机实车验证 |
| M5: 自动跟随 | ⏳ | 人走车走, 人停车停 |
| M6: 可以出门 | ⏳ | 户外各种地面稳定跟随 |
