# 架构说明 — 6轮车-UniBoard

## 概述

6轮车-UniBoard 是从"三板架构"（ESP8266 + ESP32 + OpenMV）精简为"两板架构"（ESP32 + OpenMV）的方案。ESP8266 的所有功能（电机驱动、超声波、避障）迁移到 ESP32，ESP8266 板仅保留做供电分配。

## 为什么合并

1. **消除 UART 桥接协议层** — 原方案 ESP32→ESP8266 通过 UART2 传递命令，存在延迟、丢帧、协议解析等复杂度
2. **消除 GPIO 冲突** — ESP8266 的 GPIO0/2 被电机和 SoftwareSerial 双重占用，是核心 bug 的根源
3. **减少接线** — 删除 ESP32↔ESP8266 的 3 根跳线
4. **更安全** — 避障和电机控制在同一 MCU，无通信中断风险

## 任务架构

| 任务 | 优先级 | 核心 | 职责 |
|------|--------|------|------|
| MotorTask | 3 | Core 0 | 超声波读取 + 避障状态机 + 电机控制 |
| VisionTask | 3 | Core 0 | OpenMV UART1 解析 + FollowLogic 决策 |
| WebTask | 1 | Core 1 | WiFi AP + Dashboard |

### 命令流

```
OpenMV → UART1 → VisionTask → FollowLogic → MotorCmd queue → MotorTask → MotorDriver
```

不再有 UART2 桥接、字符串协议、checksum 解析。命令通过 2 字节结构体在 FreeRTOS 队列中传递。

### 避障优先级链（MotorTask 内部）

1. 危险区 (<20cm) → 强制 STOP
2. 命令链路未建立 → STOP
3. 命令超时 (>500ms) → STOP
4. 警告区 (20-40cm) → 避障（覆盖视觉命令）
5. 正常 → 执行 FollowLogic 命令

## 已修复的 bug（从旧方案继承）

- US_SIDE_BOTH 避障退出：`||` → `&&`
- 避障超时：放弃 → 后退策略
- 0cm 视为危险距离
- FollowLogic 近距离禁转向
- FollowLogic 增加慢速过渡区间

## 文件说明

| 文件 | 来源 | 说明 |
|------|------|------|
| Config.h | 新写 | 统一引脚和参数配置 |
| MotorDriver.h/cpp | 移植自 ESP8266 | 适配 ESP32 LEDC PWM |
| UltrasonicSensors.h/cpp | 移植自 ESP8266 | 改引脚，修复 obstacleSide |
| MotorTask.h/cpp | 新写 | 避障状态机 + 电机控制任务 |
| VisionBridge.h/cpp | 直接复制 | OpenMV UART1 解析 |
| FollowLogic.h/cpp | 直接复制 | 跟随决策逻辑 |
| DataAggregator.h/cpp | 精简 | 删除 UART 解析，删除 snapshot |
| DashboardServer.h/cpp | 直接复制 | WiFi Dashboard |
| ProtocolUtils.h/cpp | 直接复制 | 校验和工具 |
| index_html.h | 直接复制 | 前端页面 |
| ESP32_Solo.ino | 新写 | 主入口，创建任务 |

## 已删除的模块

| 模块 | 原因 |
|------|------|
| UartBridge | 不再需要 UART2 |
| BridgeCommandParser | 不再需要解析命令字符串 |
| ESP8266_Car 整个目录 | ESP8266 不再作为 MCU |

---

*架构文档编写日期：2026-04-28*
