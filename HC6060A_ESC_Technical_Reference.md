# HC6060A 双向双路差速有刷电机电调 — 开发技术参考手册

> **用途**：给 Claude / 其他 AI 编程助手阅读，用于代码开发  
> **产品**：防水 HC6060A 双向双路差速有刷电调  
> **店铺**：淘宝「黑翅科技」| ¥353 起  
> **信息来源**：淘宝商品详情页截图（2026-05 确认）  
> **当前版本**：🟢 **混控款**（内置混控，无需 MCU 做差速）  
> **最后更新**：2026-05-28

---

## 0. 版本确认：混控款

你买到的是**混控款**。这意味着：

- **白线 = 油门**（两个电机同向同速，前进/后退）
- **黄线 = 转向**（两个电机差速/反向，左转/右转）
- **电调内部已经完成了差速混控**，MCU 不需要做 tank-mix 计算

> 如果以后换成独立款：白线 = 电机 A 独立控制，黄线 = 电机 B 独立控制，MCU 需要自己做混控（见 §4.6 备选代码）。

---

## 1. 产品规格 (Specifications)

| 参数 | 官方值 | 备注 |
|------|--------|------|
| **型号** | HC6060A | 双向双路有刷电调 |
| **输入电压** | DC 15V ~ <80V | ⚠️ **不能接 72V 电池**（满电超 80V） |
| **输出电流** | 60A + 60A（双路） | 单路持续 60A |
| **推荐电机电流** | ≤20A（单个电机） | 长期工作建议油门 >70% |
| **BEC 输出** | 5V / 0.3A | **仅供接收机**！不能给 MCU+传感器供电 |
| **油门行程** | **固定，不可校准** | 官方原文："电调行程固化，不能校准" |
| **信号频率** | 标准 50Hz PWM | 1000–2000 μs |
| **防水** | 是 | 防溅水，非潜水 |
| **尺寸** | 约 7.5 × 7.5 × 3.2 cm | |
| **信号线长** | 约 27 cm | |
| **动力线长** | 约 15 cm | |
| **重量** | 约 275 g | |
| **适用场景** | 履带车、差速转向设备 | |

### 油门特殊行为（硬件固化）

| 油门范围 | 模式 | 说明 |
|---------|------|------|
| **< 30% 油门** | 拖刹模式 | 松油门有刹车效果，利于转弯操控 |
| **> 30% 油门** | 无刹车模式 | 松油门靠惯性滑行，省电 |

> 这对控制算法有直接影响：低速时电机会被电调主动制动，不能期望"松油门就惯性滑行"。

---

## 2. 硬件接线 (Wiring)

### 2.1 接线端口一览

**信号端（4 线，单排 4-pin 杜邦母头）：**

| 线色 | 混控款功能 | 连接目标 |
|------|-----------|---------|
| **白** | 油门（两电机同向同速） | MCU GPIO（PWM） |
| **黄** | 转向（两电机差速/反向） | MCU GPIO（PWM） |
| **红** | BEC +5V / 0.3A | 接收机供电（⚠️不给 MCU 供电） |
| **黑** | GND | MCU GND |

**动力端（粗硅胶线，共 3 组）：**

| 线色 | 标签 | 功能 |
|------|------|------|
| **红**（粗） | — | 电池正极 + |
| **黑**（粗） | — | 电池负极 - |
| **蓝+黄**（第1组） | **AW+ / AW-** | 电机 A（无极性，方向反了对调） |
| **蓝+黄**（第2组） | **BW+ / BW-** | 电机 B（无极性，方向反了对调） |

```
                    ┌──────────────────────────┐
    电池+ ──→ 红(粗)│                          │蓝(AW+)──→ 电机 A
    电池- ──→ 黑(粗)│      HC6060A ESC         │黄(AW-)──→ 电机 A
                    │   混控款  7.5×7.5×3.2cm  │
  MCU PWM ──→ 白    │         275g            │蓝(BW+)──→ 电机 B
  (油门)            │                          │黄(BW-)──→ 电机 B
  MCU PWM ──→ 黄    │                          │
  (转向)            │                          │
  (BEC 5V) ←─ 红    │                          │
  MCU GND ──→ 黑    │                          │
                    └──────────────────────────┘
                     ↑ 信号端：白/黄/红/黑 4线
                        (单排 4-pin 杜邦头, ~27cm)
```

### 2.2 接线注意事项

1. **电源极性绝对不能反接** — 反接瞬间烧毁 MOS 管（烧驱动电路无保修）
2. 电机线如果转向反了，**对调该路蓝/黄两根线**即可
3. ⚠️ **绝对不能只接一个电机（AW 或 BW）开机，也不能不接电机开机——会损坏电调！**
4. **必须共地**：MCU 的 GND 必须与信号端的黑线连接
5. ⚠️ **BEC 只有 5V 0.3A** — MCU 请独立供电。如果 MCU 已有电源，**把信号端的红线挑出包好悬空**
6. 大电流走线用 **AWG 10–12** 硅胶线
7. 建议在电池回路串联一个 **60A 保险丝/断路器**

### 2.3 保修须知

| 故障类型 | 保修政策 |
|---------|---------|
| **烧主驱动电路**（冒烟、有异味） | **无保修、无维修** |
| 控制部分问题 | 签收 7 天内可换（外观无损、未剪线）；6 个月保修，运费 AA |
| BEC 无输出 | 维修费 80 元，买家承担运费 |
| 拆封后 | **不支持退货**。如需退货，扣除 30% 货款 |

---

## 3. PWM 信号协议

### 3.1 基本参数

```
频率:       50 Hz (周期 20ms)
脉宽范围:   1000–2000 μs
中位(停止): 1500 μs
油门行程:   固化，不可校准 ⚠️
```

> **油门行程是固化的，不能像普通航模电调那样做油门行程校准。**

### 3.2 混控款信号定义

```
白线（油门）：
    1500 μs = 停止
    >1500 μs = 两电机同时前进（越远离中位越快）
    <1500 μs = 两电机同时后退

黄线（转向）：
    1500 μs = 直行（不转向）
    >1500 μs = 右转（左轮快/右轮慢）
    <1500 μs = 左转（右轮快/左轮慢）
```

```
白线 (油门):
  1000 μs         1500 μs          2000 μs
  ├─────────────────┼─────────────────┤
  ← 全速后退        停止         全速前进 →

黄线 (转向):
  1000 μs         1500 μs          2000 μs
  ├─────────────────┼─────────────────┤
  ← 全速左转        直行         全速右转 →
```

### 3.3 油门分区（影响控制策略）

```
白线油门值:
  1000 μs                                1500 μs                                2000 μs
  ├────────────────────────────────────────┼────────────────────────────────────────┤
  │←──── 拖刹模式 (throttle <30%) ────→│←──── 无刹车模式 (throttle >30%) ────→│
  │   松油门=主动制动                     │   松油门=惯性滑行                      │
```

**对代码的影响：**
- 低速操控（<30% 油门）：电机有刹车效果，松油门即减速
- 高速操控（>30% 油门）：无刹车，纯惯性
- 如果你的路径规划/速度控制算法假设"PWM=1500 即滑行"，低速时会表现异常（实际是在刹车）

### 3.4 上电/自检时序

```
1. MCU 先上电，输出中位信号 (1500μs) 到白线和黄线
2. 确认两个电机都已接好（AW 和 BW 都必须接！）
3. 电调接通电池电源
4. 电调自检（发出提示音），约 2–3 秒
5. 自检通过后可以响应 PWM 信号
```

---

## 4. 控制代码

### 4.1 混控款核心逻辑

MCU 只需要输出两路独立 PWM，不需要自己做差速混控：

```c
// 混控款：电调内部已处理差速逻辑
// 只需要控制两个独立的维度：

// 白线 = 油门（控制速度大小和方向）
//   -1.0 (全速后退) ... 0 (停止) ... +1.0 (全速前进)
//
// 黄线 = 转向（控制转向方向和力度）
//   -1.0 (全左转) ... 0 (直行) ... +1.0 (全右转)
```

### 4.2 C 实现

```c
#include <stdint.h>

#define PWM_NEUTRAL   1500
#define PWM_MIN       1000
#define PWM_MAX       2000
#define PWM_RANGE     500

/**
 * Convert throttle and steering to PWM values for mixed-mode HC6060A.
 *
 * The ESC handles differential mixing internally.
 *
 * @param throttle  -1.0 (full reverse) to +1.0 (full forward), 0 = stop
 * @param steering  -1.0 (full left) to +1.0 (full right), 0 = straight
 * @param white_us  output: white wire PWM (throttle) in microseconds
 * @param yellow_us output: yellow wire PWM (steering) in microseconds
 */
void mixed_control(float throttle, float steering,
                   uint16_t *white_us, uint16_t *yellow_us)
{
    int32_t w = PWM_NEUTRAL + (int32_t)(throttle * PWM_RANGE);
    int32_t y = PWM_NEUTRAL + (int32_t)(steering * PWM_RANGE);

    if (w > PWM_MAX) w = PWM_MAX;
    if (w < PWM_MIN) w = PWM_MIN;
    if (y > PWM_MAX) y = PWM_MAX;
    if (y < PWM_MIN) y = PWM_MIN;

    *white_us  = (uint16_t)w;
    *yellow_us = (uint16_t)y;
}
```

### 4.3 Python 实现

```python
PWM_NEUTRAL = 1500
PWM_RANGE = 500
PWM_MIN = 1000
PWM_MAX = 2000

def mixed_control(throttle: float, steering: float) -> tuple[int, int]:
    """
    Mixed version (混控款). ESC handles differential mixing.

    Args:
        throttle: -1.0 (full reverse) to +1.0 (full forward), 0 = stop
        steering: -1.0 (full left) to +1.0 (full right), 0 = straight
    Returns:
        (white_wire_pwm, yellow_wire_pwm)
    """
    w = PWM_NEUTRAL + int(throttle * PWM_RANGE)
    y = PWM_NEUTRAL + int(steering * PWM_RANGE)
    return (
        max(PWM_MIN, min(PWM_MAX, w)),
        max(PWM_MIN, min(PWM_MAX, y))
    )


# Test
if __name__ == "__main__":
    print(mixed_control(1.0, 0.0))    # (2000, 1500) 全速前进直行
    print(mixed_control(0.0, 1.0))    # (1500, 2000) 原地右转
    print(mixed_control(0.5, 0.0))    # (1750, 1500) 半速前进直行
    print(mixed_control(0.5, 0.5))    # (1750, 1750) 半速前进+右转
    print(mixed_control(0.0, -1.0))   # (1500, 1000) 原地左转
    print(mixed_control(-1.0, 0.0))   # (1000, 1500) 全速后退直行
```

### 4.4 行为矩阵

| throttle | steering | 白线 (μs) | 黄线 (μs) | 车辆行为 |
|----------|----------|----------|----------|---------|
| +1.0 | 0.0 | 2000 | 1500 | 全速前进直行 |
| +0.5 | 0.0 | 1750 | 1500 | 半速前进直行 |
| 0.0 | 0.0 | 1500 | 1500 | 静止 |
| -1.0 | 0.0 | 1000 | 1500 | 全速后退直行 |
| +0.5 | +0.5 | 1750 | 1750 | 半速前进+右转 |
| +0.5 | -0.5 | 1750 | 1250 | 半速前进+左转 |
| 0.0 | +1.0 | 1500 | 2000 | 原地右转 |
| 0.0 | -1.0 | 1500 | 1000 | 原地左转 |
| +1.0 | +1.0 | 2000 | 2000 | 全速前进+全右转 |

> 注意：混控款的黄线转向力度由电调内部决定。`steering=0.5` 到底会让差速多剧烈，取决于电调的混控比例，**需要实测标定**。

### 4.5 转向标定建议

电调内部的混控比例是未知的。建议做一个标定流程：

```python
def calibrate_steering(control_func):
    """
    逐步增加 steering 值，找到最小能完成原地旋转的 steering。
    记录为 MIN_TURN，然后 scale 你的 steering 输入。
    """
    for s in [0.1, 0.15, 0.2, 0.25, 0.3, 0.4, 0.5]:
        white, yellow = control_func(0.8, s)
        # 发送 PWM，观察车辆是否开始原地旋转
        # 记录最小值
```

### 4.6 备选：如果你以后换独立款

```python
# 独立款需要 MCU 自己做混控
def differential_mix(throttle: float, steering: float) -> tuple[int, int]:
    """
    Independent version (独立款). MCU does the mixing.
    
    白线 = 电机 A (AW), 黄线 = 电机 B (BW)
    """
    a = throttle + steering  # motor A = throttle + steer
    b = throttle - steering  # motor B = throttle - steer

    def scale(x: float) -> int:
        pulse = PWM_NEUTRAL + int(x * PWM_RANGE)
        return max(PWM_MIN, min(PWM_MAX, pulse))

    return scale(a), scale(b)
```

---

## 5. MCU 平台完整实现（混控款）

### 5.1 Arduino

接线：**白线→D9（油门），黄线→D10（转向），黑线→GND**。MCU 独立供电（不用红线）。

```cpp
#include <Servo.h>

Servo escThrottle, escSteering;  // 白线=油门, 黄线=转向

#define PIN_THROTTLE  9
#define PIN_STEERING  10
#define NEUTRAL       1500

void setup() {
  escThrottle.attach(PIN_THROTTLE);
  escSteering.attach(PIN_STEERING);
  escThrottle.writeMicroseconds(NEUTRAL);
  escSteering.writeMicroseconds(NEUTRAL);
  delay(3000);
  Serial.begin(115200);
}

// throttle: -1.0..+1.0,  steering: -1.0..+1.0
void setControl(float throttle, float steering) {
  int w = constrain(NEUTRAL + (int)(throttle * 500), 1000, 2000);
  int y = constrain(NEUTRAL + (int)(steering * 500), 1000, 2000);
  escThrottle.writeMicroseconds(w);
  escSteering.writeMicroseconds(y);
}

void loop() {
  setControl(0.5, 0.0);   // 半速前进直行
  delay(2000);
  setControl(0.0, 0.0);   // 停止
  delay(1000);
}
```

### 5.2 STM32 (HAL)

接线：**白线→TIMx_CH1（油门），黄线→TIMx_CH2（转向），黑线→GND**。

```c
// Timer: PSC=84-1, ARR=20000-1 → 50Hz
#define PWM_NEUTRAL 1500

void esc_init(TIM_HandleTypeDef *htim) {
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, PWM_NEUTRAL);
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, PWM_NEUTRAL);
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_2);
    HAL_Delay(3000);
}

void esc_set(TIM_HandleTypeDef *htim,
             float throttle, float steering) {
    int32_t w = PWM_NEUTRAL + (int32_t)(throttle * 500);
    int32_t y = PWM_NEUTRAL + (int32_t)(steering * 500);
    if (w < 1000) w = 1000; if (w > 2000) w = 2000;
    if (y < 1000) y = 1000; if (y > 2000) y = 2000;
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, (uint16_t)w);
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, (uint16_t)y);
}
```

### 5.3 ESP32 (LEDC)

接线：**白线→GPIO17（油门），黄线→GPIO18（转向），黑线→GND**。

```cpp
#include <driver/ledc.h>

#define LEDC_TIMER   LEDC_TIMER_0
#define LEDC_MODE    LEDC_LOW_SPEED_MODE
#define LEDC_CH_W    LEDC_CHANNEL_0  // 白线=油门
#define LEDC_CH_Y    LEDC_CHANNEL_1  // 黄线=转向
#define LEDC_RES     LEDC_TIMER_14_BIT
#define LEDC_FREQ    50

static uint32_t us_to_duty(int us) {
    return (uint32_t)((us / 20000.0) * 16384);
}

void esc_init(int gpio_w, int gpio_y) {
    ledc_timer_config_t t = {
        .speed_mode = LEDC_MODE, .duty_resolution = LEDC_RES,
        .timer_num = LEDC_TIMER, .freq_hz = LEDC_FREQ, .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&t);

    ledc_channel_config_t c = {.speed_mode = LEDC_MODE, .timer_sel = LEDC_TIMER,
                                .duty = 0, .hpoint = 0};
    c.gpio_num = gpio_w; c.channel = LEDC_CH_W; ledc_channel_config(&c);
    c.gpio_num = gpio_y; c.channel = LEDC_CH_Y; ledc_channel_config(&c);

    uint32_t n = us_to_duty(1500);
    ledc_set_duty(LEDC_MODE, LEDC_CH_W, n); ledc_update_duty(LEDC_MODE, LEDC_CH_W);
    ledc_set_duty(LEDC_MODE, LEDC_CH_Y, n); ledc_update_duty(LEDC_MODE, LEDC_CH_Y);
    vTaskDelay(pdMS_TO_TICKS(3000));
}

void esc_set(float throttle, float steering) {
    int w = 1500 + (int)(throttle * 500); if (w < 1000) w = 1000; if (w > 2000) w = 2000;
    int y = 1500 + (int)(steering * 500); if (y < 1000) y = 1000; if (y > 2000) y = 2000;
    ledc_set_duty(LEDC_MODE, LEDC_CH_W, us_to_duty(w)); ledc_update_duty(LEDC_MODE, LEDC_CH_W);
    ledc_set_duty(LEDC_MODE, LEDC_CH_Y, us_to_duty(y)); ledc_update_duty(LEDC_MODE, LEDC_CH_Y);
}
```

### 5.4 Raspberry Pi (pigpio)

接线：**白线→GPIO17（油门），黄线→GPIO18（转向），黑线→GND**。MCU 独立供电。

```python
import pigpio, time

PIN_THROTTLE, PIN_STEERING = 17, 18
NEUTRAL = 1500

pi = pigpio.pi()
pi.set_servo_pulsewidth(PIN_THROTTLE, NEUTRAL)
pi.set_servo_pulsewidth(PIN_STEERING, NEUTRAL)
time.sleep(3)

def set_control(throttle: float, steering: float):
    w = max(1000, min(2000, int(NEUTRAL + throttle * 500)))
    y = max(1000, min(2000, int(NEUTRAL + steering * 500)))
    pi.set_servo_pulsewidth(PIN_THROTTLE, w)
    pi.set_servo_pulsewidth(PIN_STEERING, y)
```

---

## 6. 电机与电池选型

### 6.1 官方推荐

| 电池电压 | 推荐电机功率 | 限制条件 |
|---------|------------|---------|
| 24V | ≤350W | 电机转速 <4000 RPM |
| 36V | ≤500W | 电机转速 <4000 RPM |
| 48V | ≤650W | 电机转速 <4000 RPM |
| 60V | ≤800W | 电机转速 <4000 RPM |

> 如果不是电动车专用电机，需咨询客服。

### 6.2 电压限制

- **下限**：DC 15V
- **上限**：< DC 80V
- ⚠️ **不能接 72V 标称电池**（满电超 80V）

锂电池参考：

| 目标电压 | 推荐 S 数 | 满电电压 | 安全 |
|---------|----------|---------|------|
| 36V | 10S | 42.0V | ✅ |
| 48V | 13S | 54.6V | ✅ |
| 60V | 16S | 67.2V | ✅ |
| 72V | 20S | 84.0V | ❌ |

### 6.3 电机要求

- **类型**：有刷直流电机
- **额定电流**：单电机 ≤20A
- **转速**：<4000 RPM
- **长期工作**：建议油门 >70%

---

## 7. 故障排查

| 现象 | 可能原因 |
|------|---------|
| 上电无反应 | 电压超范围 (<15V 或 >80V)；极性反接；烧毁 |
| 电机不转 | 只接了一个电机（AW/BW 缺一）；PWM 不在中位 |
| 转向方向反了 | 对调黄线的正负逻辑（steering 取反） |
| 前进方向反了 | 对调两路电机线（蓝/黄对调） |
| 转弯行为不符合预期 | **需要标定**：电调内部混控比例未知，实测确定 |
| 低速刹车太猛 | 这是 <30% 油门的拖刹模式，设计如此 |
| BEC 无输出 | 过载/短路，维修 80 元 |
| 冒烟/异味 | 主驱动烧毁，**无保修** |

---

## 8. 遥控器使用（不用 MCU 时）

| 信号线 | 连接接收机 | 说明 |
|--------|----------|------|
| **白** | 油门通道（通常是 CH2/CH3） | 控制前进/后退速度 |
| **黄** | 转向通道（通常是 CH1） | 控制左转/右转 |
| **红** | 接收机 5V 供电 | BEC 0.3A，不能接其他电源 |
| **黑** | 接收机 GND | 共地 |

> 混控款不需要遥控器支持混控——电调内部已经做了。

---

## 9. Bring-up Checklist

- [ ] 1. 确认是混控款 ✅
- [ ] 2. 两个电机都已接好（AW 和 BW 缺一不可）
- [ ] 3. 电池电压在 15V~<80V（不是 72V）
- [ ] 4. 电源正负极正确（红+黑-）
- [ ] 5. MCU 独立供电（不用 BEC 红线，或挑出悬空）
- [ ] 6. MCU 先输出 1500μs 到白线和黄线
- [ ] 7. 接通动力电池，听到自检音
- [ ] 8. 小油门测试：白线→1600μs，确认两电机同时前进
- [ ] 9. 转向测试：黄线→1600μs（白线保持 1500），确认原地转向
- [ ] 10. 标定 steering 值，确定转弯灵敏度

---

## 10. 安全警告

1. ⚠️ **绝对不能只接一个电机或不接电机上电** — 会损坏
2. **72V 电池禁止** — 满电超硬上限 80V
3. **BEC 0.3A 仅供接收机** — 不供 MCU
4. **烧主驱动无保修**
5. **油门行程不可校准** — 别做传统校准操作
6. **拆封后不退货** — 购买前确认需求
7. 大电流（60A+）足以引起火灾，接头牢固、线径足够
8. 锂电池必须有 BMS，建议串急停开关

---

## 参考

- 淘宝店铺：搜索「黑翅科技」
- 商品标题：「防水36V48V60V双向双路差速有刷电机电调 HC6060A 履带4S-17S高压」

---

*本文档基于淘宝商品详情页截图（2026-05）和用户实物确认（混控款）。参数以实物和卖家最新说明为准。*
