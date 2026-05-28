# ESP32_Solo — 已废弃 (Deprecated)

**废弃日期**: 2026-05-28
**替代方案**: ESP8266 Bridge 已升级为全功能控制器，替代 ESP32 全部功能。

## 迁移说明

ESP32 的功能已全部迁移至 ESP8266 (NodeMCU V3):

| 功能 | ESP32 (旧) | ESP8266 (新) |
|------|-----------|-------------|
| VIS 接收 | Serial2 (GPIO16, 硬件异常) | SoftwareSerial D5/GPIO14 |
| FollowLogic | Core 0 FreeRTOS task | 主循环同步调用 |
| MotorCmd 输出 | Serial2 → STM32 | UART0 swapped (D7/D8) → STM32 |
| WiFi Dashboard | AP: "Tracked Robot" | AP: "Rover" |
| CRSF 遥控 | 未实现 | 未实现 (Phase 1+) |

## 保留原因

ESP32_Solo/ 目录保留作为参考实现:
- FollowLogic 算法参考
- DataAggregator 架构参考
- VisionBridge 协议解析参考

如需在未来恢复 ESP32 (例如需要双核 FreeRTOS 或蓝牙功能)，
可参考 ESP8266_Bridge/ 的最新 FollowLogic + MotorCmd 实现。

## 接线变更

旧 (ESP32): GPIO17(TX) → STM32 PB11, GPIO16(RX) → STM32 PB10
新 (ESP8266): D8/GPIO15(TX) → STM32 PB11, D7/GPIO13(RX) → STM32 PB10
