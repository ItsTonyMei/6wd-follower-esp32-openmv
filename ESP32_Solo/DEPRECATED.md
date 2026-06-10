# ESP32_Solo — 主 L2 控制器

**重新启用日期**: 2026-06-03 (恢复为主控制器)
**此前废弃**: 2026-05-28 (曾因 GPIO16 疑似异常被 ESP8266 替代)
**当前状态**: 精简固件就绪，作为主控制器运行。ESP8266 保留为下位备用硬件。

## 与 ESP8266 对比

| 功能 | ESP8266 | ESP32 (精简固件) |
|------|---------|-----------------|
| VIS 接收 | D5/GPIO14 (SoftwareSerial) | GPIO4 (SoftwareSerial) |
| STM32 TX | D8/GPIO15 (UART0 swapped) | GPIO17 (Serial2) |
| STM32 RX | D7/GPIO13 (UART0 swapped) | GPIO16 (Serial2) |
| Debug | 无 (UART0 已释放) | GPIO1/3 (Serial USB) |
| LED 指示 | GPIO2 (LED_BUILTIN) | GPIO2 (PIN_LED, active-HIGH) |
| FollowLogic | 主循环同步调用 | 主循环同步调用 (相同代码) |
| MotorCmd 输出 | 6-byte CRC8 | 6-byte CRC8 (相同协议) |
| WiFi Dashboard | AP: "Tracked Robot" | AP: "Tracked Robot" (相同) |
| CRSF 遥控 | 不支持 | 预留 UART1 (GPIO4/15, 待 Phase 2) |

## 硬件状态 (2026-06-03 验证)

ESP32-WROOM-32U (DevKit V1, D0WD-V3 v3.1) — 全部外设正常:

| 测试项 | 引脚 | 结果 |
|--------|------|------|
| UART0 TX | GPIO1 | ✅ 正常 (USB-Serial debug) |
| UART0 RX | GPIO3 | ✅ 正常 |
| UART2 TX | GPIO17 | ✅ 正常 (→ STM32 PB11) |
| UART2 RX | GPIO16 | ✅ 正常 (← STM32 PB10) |
| GPIO2 LED | GPIO2 | ✅ 正常 (板载 LED, active-HIGH) |

> **烧录注意事项**: ESP32 的 stub loader 无法正常加载 (no sync reply)，需使用 ROM bootloader 烧录。
> ```
> arduino-cli upload -p COM9 --fqbn esp32:esp32:esp32 \
>   --upload-property "upload.flags=--no-stub" \
>   --board-options "UploadSpeed=115200" <sketch>
> ```

## LED 指示行为

| LED 状态 | 含义 |
|----------|------|
| 常亮 (HIGH) | ESP32 运行中，等待 VIS 数据 |
| 闪烁 (LOW pulse) | 正在接收 VIS 帧 (每帧一瞬) |
| 常灭 (LOW) | VIS 曾收到过 (累计指示) |

## 固件架构 (精简版)

单线程 `loop()` 架构，无需 FreeRTOS:
```
VIS(SoftwareSerial) → parseVisFrame() → FollowLogic.update()
                                            ↓
WiFi Dashboard ← handleStatus()      MotorCmd → Serial2 → STM32
```

源文件 (仅 4 个):
- `ESP32_Solo.ino` — 主程序 (VIS + FollowLogic + Dashboard + MotorCmd)
- `Config.h` — 引脚/参数配置
- `FollowLogic.cpp/h` — 跟随决策算法 (与 ESP8266 完全相同)

## 废弃记录 (历史)

- **2026-05-28**: ESP32 因 GPIO16 疑似异常被 ESP8266 替代
- **2026-06-03**: 验证 GPIO16 正常，精简代码至 4 文件，重新启用为备用控制器
- **2026-06-03**: 确认 GPIO2 板载 LED 正常 (active-HIGH)
