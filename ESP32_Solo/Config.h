#pragma once
#include <Arduino.h>

// ============================================================================
// 履带车视觉跟随系统 — ESP32 配置
// 所有引脚、阈值、任务参数的单一起源 (single source of truth)
// ============================================================================
//
// 架构: OpenMV (L1感知) → ESP32-WROOM-32U DevKit V1 (L2决策) → STM32F103C8T6 (L3执行+安全)
// 电机 PWM 直驱已迁移至 STM32，ESP32 通过 UART2 MotorCmd 间接控制。
// 超声波 HC-SR04 已全部移除，距离测量由 OpenMV VL53L1X ToF 测距扩展板 (I2C) 负责。
// ToF 距离数据 (mm) 通过 VIS 帧 distScore 字段融合传入 ESP32。
//
// 供电: 48V 89Ah 锂电池 → 48V→5V 10A 防水降压 → ESP32 (5V USB)

// ─── UART 引脚分配 ───
// ┌──────────┬────────────────┬──────────────────────────────────────────┐
// │  接口     │ 引脚            │ 用途                                     │
// ├──────────┼────────────────┼──────────────────────────────────────────┤
// │ UART0    │ GPIO1/3 (固定)  │ USB Serial — debug/monitor               │
// │ UART1    │ RX=15, TX=4     │ ELRS 接收机 CRSF 协议 (420k baud)        │
// │ UART2    │ TX=17, RX=16    │ STM32 USART3 双向 MotorCmd+遥测 (115200) │
// │ (Phase0) │ ESP8266 WiFi    │ VIS 数据经 ESP8266 Bridge 转发            │
// └──────────┴────────────────┴──────────────────────────────────────────┘

// ─── UART1: ELRS/CRSF 遥控接收机 ───
constexpr uint8_t PIN_CRSF_RX       = 15;   // ← ELRS RX CRSF TX
constexpr uint8_t PIN_CRSF_TX       = 4;    // → ELRS RX CRSF RX (双向遥测)
constexpr uint32_t CRSF_BAUD        = 420000;

// ─── UART2: STM32 双向通信 (MotorCmd 下行 + 遥测上行) ───
// Phase 0: VIS 经 ESP8266 WiFi 桥接, Serial2 专用于 STM32 通信
constexpr uint8_t PIN_STM32_TX      = 17;   // → STM32 USART3 RX (PB11)
constexpr uint8_t PIN_STM32_RX      = 16;   // ← STM32 USART3 TX (PB10)
constexpr uint32_t STM32_UART_BAUD  = 115200;

// ─── CRSF 遥控通道映射 (Jumper T14 EdgeTX Mixer) ───
// CH1: 右摇杆 X → Steering (转向)
// CH2: 备用 (摄像头俯仰预留)
// CH3: 左摇杆 Y → Throttle (油门, 中位=0)
// CH4: 备用
// CH5: SWA 三档 → Mode: Manual / Auto-Follow / E-Stop
// CH6: POT1 旋钮 → Max Speed 限速
constexpr uint8_t  CRSF_CH_STEERING    = 1;
constexpr uint8_t  CRSF_CH_THROTTLE    = 3;
constexpr uint8_t  CRSF_CH_MODE_SWITCH = 5;
constexpr uint8_t  CRSF_CH_SPEED_LIMIT = 6;

// CRSF 通道值参数
// 原始范围: 172~1811 (11-bit), 中位 ~992
// 归一化后 → -1.0 ~ 0 ~ 1.0 (中位=0)
constexpr uint16_t CRSF_RAW_MIN    = 172;
constexpr uint16_t CRSF_RAW_MAX    = 1811;
constexpr uint16_t CRSF_RAW_MID    = 992;
constexpr float    CRSF_DEADBAND   = 0.03f;  // 摇杆中位 ±3% 视为 STOP

// ─── CRSF 链路安全参数 ───
constexpr uint32_t CRSF_FRAME_TIMEOUT_MS = 100;  // CRSF 帧中断超时 → STOP
constexpr uint8_t  CRSF_LQ_MIN           = 50;   // LQ < 50% → Dashboard 警告

// ─── STM32 通信参数 ───
constexpr uint32_t STM32_CMD_INTERVAL_MS  = 50;   // MotorCmd 发送间隔
constexpr uint32_t STM32_TELEM_TIMEOUT_MS = 300;  // 遥测超时 → Dashboard 报警

// ─── 命令超时 (ms): ESP32 → STM32 无命令 → STM32 自动 STOP ───
constexpr unsigned long COMMAND_TIMEOUT_MS = 500;

// ─── WiFi AP 配置 ───
constexpr char WIFI_SSID[] = "Tracked Robot";
constexpr char WIFI_PASS[] = "12345678";

// ─── FreeRTOS task stack size ───
constexpr uint32_t TASK_STACK_SIZE = 4096;

// ─── 视觉数据超时 (ms): 超过此时间无 VIS 帧 → 视为失效 ───
constexpr unsigned long VISION_TIMEOUT_MS = 700;

// ─── 距离保持阈值 (distScore 0.0-1.0) ───
// distScore 由 OpenMV 端视觉特征 (area_ratio, feetY) + VL53L1X ToF 距离 (mm) 融合计算
// ESP32 FollowLogic 仅使用最终 distScore 做决策，不直接感知 ToF 原始数据
// 阈值在履带车上需重新标定（Phase 6.4）
constexpr float DIST_FAR         = 0.25f;  // < this → 全速追赶
constexpr float DIST_APPROACHING = 0.45f;  // < this → 中速跟近
constexpr float DIST_NEAR_START  = 0.60f;  // > this → 开始后退
constexpr float DIST_NEAR        = 0.80f;  // > this → 全速后退
constexpr float DIST_HYSTERESIS  = 0.05f;  // 滞后带（防振荡）
constexpr int   DIST_STABLE_FRAMES = 3;    // 状态切换需维持帧数

// ─── HC6060A 混控款电调 PWM 参数 ───
// 白线=油门, 黄线=转向, 电调内部处理差速混控
// 50Hz 标准舵机 PWM: 1000-2000μs, 中位 1500μs
constexpr uint16_t PWM_NEUTRAL         = 1500;
constexpr uint16_t PWM_MIN             = 1000;
constexpr uint16_t PWM_MAX             = 2000;
constexpr uint16_t MAX_THROTTLE_OFFSET = 400;   // 最大油门偏中位 (1500±400 = 1100~1900μs)
constexpr uint16_t MAX_STEER_OFFSET    = 300;   // 最大转向偏中位 (1500±300 = 1200~1800μs)
constexpr uint16_t THROTTLE_DEADBAND   = 20;    // 油门中位死区 (±20μs, 防止漂移)

// ─── 履带差速转向参数 ───
constexpr float TURN_KP          = 2.0f;   // 转向 P 增益 (cx 偏差像素 → steering μs)
constexpr int   CX_MIN_OFFSET    = 15;     // cx 偏差绝对值 < this → 不转向 (中心死区)

// ─── MotorCmd: FreeRTOS queue 元素 (4 字节, 零 heap 分配) ───
// HC6060A 混控款: throttle/steering 直接对应白线/黄线 PWM 脉宽 (μs)
struct MotorCmd {
    uint16_t throttle;  // 白线油门 (1000-2000μs, 1500=停止)
    uint16_t steering;  // 黄线转向 (1000-2000μs, 1500=直行)
};
