# 架构说明 — 6WD Follower UniBoard

## 概述

6WD Follower 是从"三板架构"（ESP8266 + ESP32 + OpenMV）精简为"两板架构"（ESP32 + OpenMV）的方案。ESP8266 的所有功能（电机驱动、超声波、避障）迁移到 ESP32，ESP8266 板仅保留做供电分配。

## 为什么合并

1. **消除 UART 桥接协议层** — 原方案 ESP32→ESP8266 通过 UART2 传递命令，存在延迟、丢帧、协议解析等复杂度
2. **消除 GPIO 冲突** — ESP8266 的 GPIO0/2 被电机和 SoftwareSerial 双重占用，是核心 bug 的根源
3. **减少接线** — 删除 ESP32↔ESP8266 的 3 根跳线
4. **更安全** — 避障和电机控制在同一 MCU，无通信中断风险

## 任务架构 (Task Architecture)

| 任务 | 优先级 | 核心 | 职责 |
|------|--------|------|------|
| MotorTask | 3 | Core 0 | 超声波读取 + 避障状态机 + 电机控制 |
| VisionTask | 3 | Core 0 | OpenMV UART1 解析 + FollowLogic 决策 |
| WebTask | 1 | Core 1 | WiFi AP + Dashboard |

### 命令流 (Command Flow)

```
OpenMV → UART1 → VisionTask (VisionBridge 解析)
       → FollowLogic::update() → MotorCmd
       → xQueueSend(motorCmdQueue)
       → MotorTask::executeMotorCmd() → MotorDriver
```

不再有 UART2 桥接、字符串协议、checksum 解析。命令通过 2 字节 MotorCmd struct 在 FreeRTOS queue 中传递。

### 避障优先级链 (Obstacle Avoidance Priority Chain, MotorTask 内部)

1. 危险区 (<20cm) → 强制 STOP
2. 命令链路未建立 → STOP
3. 命令超时 (>500ms) → STOP
4. 警告区 (20-40cm) → 避障转向 / 超时后退
5. 正常 → 执行 FollowLogic 下发的 MotorCmd

## 已修复的 bug (从旧方案继承)

- US_SIDE_BOTH 避障退出条件: `||` → `&&`
- 避障超时: 放弃 → 后退策略
- 0cm 视为危险距离
- FollowLogic 近距离禁转向 (防止撞到人)
- FollowLogic 增加慢速过渡区间 (SLOW_SPEED)

## 文件说明

| 文件 | 来源 | 说明 |
|------|------|------|
| Config.h | 新写 | 统一引脚和参数配置 |
| MotorDriver.h/cpp | 移植自 ESP8266 | 适配 ESP32 LEDC PWM, 使用 Config.h MOTOR_DEAD_TIME_MS |
| UltrasonicSensors.h/cpp | 移植自 ESP8266 | 改引脚，修复 obstacleSide |
| MotorTask.h/cpp | 新写 | 避障状态机 + 电机控制任务, 使用 DataAggregator lock/unlock |
| VisionBridge.h/cpp | 重构 | UART1 VIS 协议解析, strtol/strtof 代替 const_cast |
| FollowLogic.h/cpp | 重构 | 跟随决策, 直接返回 MotorCmd struct (不再返回字符串) |
| DataAggregator.h/cpp | 重构 | 封装互斥锁, snprintf 优化 JSON 生成 |
| DashboardServer.h/cpp | 重构 | WiFi Dashboard, 使用 DataAggregator lock/unlock |
| ProtocolUtils.h/cpp | 保持 | XOR checksum 验证 |
| Tasks.h/cpp | 重构 | 删除 parseFollowCmd() 和静态 lockAggregator |
| index_html.h | 保持 | 前端页面 (HTML/CSS/JS) |
| ESP32_Solo.ino | 重构 | 主入口, 删除全局 aggregatorMutex, 依赖注入 |

## 已删除的模块

| 模块 | 原因 |
|------|------|
| UartBridge | 不再需要 UART2 (ESP32↔ESP8266) |
| BridgeCommandParser | 不再需要解析命令字符串 |
| parseFollowCmd() | FollowLogic 直接返回 MotorCmd struct |
| 静态 lockAggregator | 统一移至 DataAggregator |
| turnCommand() | 死代码, 从未调用 |
| ESP8266_Car 整个目录 | ESP8266 不再作为 MCU |

---

*架构文档更新日期：2026-05-20*
