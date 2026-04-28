# 多 Agent 分工任务指南

> 创建时间：2026.04.28
> 适用场景：三个 Agent 并行优化不同功能模块

---

## 分工方案

| Agent | 负责模块 | 目标文件 |
|-------|---------|---------|
| Codex | 电机驱动 + 跟随逻辑 | `MotorDriver.cpp/h`、`FollowLogic.cpp/h`、`Config.h` |
| Claude | 视觉通信 + Web仪表盘 | `VisionBridge.cpp/h`、`DashboardServer.cpp/h`、`index_html.h` |
| CodeBuddy | 传感器 + 避障安全 | `UltrasonicSensors.cpp/h`、`MotorTask.cpp/h`（避障部分）、`Config.h` |

---

## 原则

**各 Agent 的目标文件必须没有重叠。** merge 零冲突的前提是文件级完全不交叉。

---

## Codex → 电机驱动 + 跟随逻辑

```
仓库路径：/mnt/c/Users/meiba/Desktop/6轮车-UniBoard
起始分支：master
工作分支：work/codex-motor

目标文件（只改这些，不要碰其他文件）：
- ESP32_Solo/MotorDriver.cpp
- ESP32_Solo/MotorDriver.h
- ESP32_Solo/FollowLogic.cpp
- ESP32_Solo/FollowLogic.h
- ESP32_Solo/Config.h

优化方向：
1. 电机驱动：消除 PWM 输出重复逻辑，优化死区处理
2. 跟随逻辑：参考 MotorCmd 结构体返回方式改进类型安全
3. 代码可读性：提取常量、消除魔法数字

操作步骤：
1. git checkout -b work/codex-motor
2. 阅读目标文件的现有代码
3. 进行优化改进
4. git add -A && git commit -m "refactor: 电机驱动+跟随逻辑优化"
5. git log --oneline -3 确认提交成功
```

---

## Claude → 视觉通信 + Web仪表盘

```
仓库路径：/mnt/c/Users/meiba/Desktop/6轮车-UniBoard
起始分支：master
工作分支：work/claude-vision

目标文件（只改这些，不要碰其他文件）：
- ESP32_Solo/VisionBridge.cpp
- ESP32_Solo/VisionBridge.h
- ESP32_Solo/DashboardServer.cpp
- ESP32_Solo/DashboardServer.h
- ESP32_Solo/index_html.h

优化方向：
1. VisionBridge：优化 UART 数据解析逻辑，增强容错
2. DashboardServer：简化 HTTP 请求处理，提升响应效率
3. index_html.h：优化前端图表渲染性能，整理 CSS 结构

操作步骤：
1. git checkout -b work/claude-vision
2. 阅读目标文件的现有代码
3. 进行优化改进
4. git add -A && git commit -m "refactor: 视觉通信+Web仪表盘优化"
5. git log --oneline -3 确认提交成功
```

---

## CodeBuddy → 传感器 + 避障安全

```
仓库路径：/mnt/c/Users/meiba/Desktop/6轮车-UniBoard
起始分支：master
工作分支：work/codebuddy-sensor

目标文件（只改这些，不要碰其他文件）：
- ESP32_Solo/UltrasonicSensors.cpp
- ESP32_Solo/UltrasonicSensors.h
- ESP32_Solo/MotorTask.cpp（仅避障相关逻辑部分）
- ESP32_Solo/Config.h

优化方向：
1. 超声波：整合 ISR 非阻塞实现、传感器故障 l<0 视为危险
2. 避障逻辑：增加 CLEAR_THRESHOLD hysteresis 防止边界震荡
3. 安全增强：故障时倾向停车而非继续运行

操作步骤：
1. git checkout -b work/codebuddy-sensor
2. 阅读目标文件的现有代码
3. 进行优化改进
4. git add -A && git commit -m "refactor: 传感器+避障安全增强"
5. git log --oneline -3 确认提交成功
```

---

## 合并操作（三个 Agent 都完成后执行）

```bash
cd /mnt/c/Users/meiba/Desktop/6轮车-UniBoard

# 确认三个分支改动不重叠
git diff --name-only master..work/codex-motor
git diff --name-only master..work/claude-vision
git diff --name-only master..work/codebuddy-sensor

# 如果没有重叠 → 依次合并
git checkout master
git merge work/codex-motor --no-edit
git merge work/claude-vision --no-edit
git merge work/codebuddy-sensor --no-edit

# 确认合并结果
git log --oneline -6
```

---

## 旧分支清理（可选）

合并完成后，旧的 work 分支可以删除：

```bash
git branch -d work/claude    # 已合并到 master，可删
git branch -d work/codebuddy # 存档或删除
git branch -d work/codex     # 存档或删除
```
