/**
 * STM32F103C8T6 — L3 执行与安全层 (HC6060A 混控款电调)
 *
 * 功能:
 *   1. PS2 手柄控制 (CN4: PB12-PB15) — 手动遥控, 优先于 ESP8266
 *   2. USART3 (PB10=TX, PB11=RX) 接收 ESP8266 MotorCmd 下行协议
 *   3. TIM4 CH3 (PB8) → 黄线 (转向), CH4 (PB9) → 白线 (油门)
 *   4. 50Hz 标准舵机 PWM (1000-2000μs, 中位 1500μs)
 *   5. 500ms 命令超时 → 自动归中 (FAILSAFE)
 *   6. LED2 (PA4, 蓝灯) 模式指示 + 蜂鸣器 (PA3, 经跳线, active-HIGH)
 *
 * 控制优先级: PS2 手柄 (已连接时) > ESP8266 串口
 *
 * 定制板 (C06B):
 *   MCU: STM32F103C8T6 (Cortex-M3, 72MHz, 64KB Flash, 20KB SRAM)
 *   USB-UART: CH9102 via USART1 (PA9=TX, PA10=RX) — debug/monitor
 *   PS2: CN4 6P (PB12=CLK, PB13=CS, PB14=CMD, PB15=DATA) — WHEELTEC 定义
 *   ESP8266: USART3 (PB10=TX, PB11=RX) ← ESP8266 D7/D8
 *   ESC: PB8=黄线(转向), PB9=白线(油门) @ H6/H7 舵机接口
 *   烧录: PlatformIO serial @ 115200, -dtr,rts,dtr,,,,
 */

#include <Arduino.h>

// ─── PWM 常量 ───
constexpr uint16_t PWM_NEUTRAL    = 1500;
constexpr uint16_t PWM_MIN        = 1000;
constexpr uint16_t PWM_MAX        = 2000;
constexpr uint16_t MAX_THROTTLE   = 400;   // 油门最大偏离中位
constexpr uint16_t MAX_STEER      = 300;   // 转向最大偏离中位
constexpr int      JOY_DEADBAND   = 5;     // 摇杆中位死区 (±5 / 128)
constexpr uint32_t CMD_TIMEOUT_MS = 500;
constexpr uint32_t ESC_INIT_DELAY = 3000;

constexpr uint8_t PIN_LED2 = PA4;
// PA3 蜂鸣器 (经跳线→S8050 驱动, active-HIGH: PA3=1 响)
#define BEEP_ON  GPIOA->BSRR = (1 << 3)
#define BEEP_OFF GPIOA->BRR  = (1 << 3)

// ─── USART1: USB 调试 ───
HardwareSerial SerialUSART1(PA10, PA9);
#define Serial SerialUSART1

// ─── 全局控制状态 ───
static uint16_t  g_throttle   = PWM_NEUTRAL;
static uint16_t  g_steering   = PWM_NEUTRAL;
static uint32_t  g_lastCmdMs  = 0;
static bool      g_escReady   = false;
static bool      g_motorArmed = false;
static uint8_t   g_ps2_lx     = 128;
static uint8_t   g_ps2_ly     = 128;

// ─── PS2 引脚宏 (C06B CN4: PB15=DATA, PB14=CMD, PB13=CS, PB12=CLK) ───
// 来源: WHEELTEC C06B PS2 示例源码 (pstwo.h)
#define PS2_DAT     ((GPIOB->IDR >> 15) & 1)
#define PS2_CMD_H   GPIOB->BSRR = (1 << 14)
#define PS2_CMD_L   GPIOB->BRR  = (1 << 14)
#define PS2_CS_H    GPIOB->BSRR = (1 << 13)
#define PS2_CS_L    GPIOB->BRR  = (1 << 13)
#define PS2_CLK_H   GPIOB->BSRR = (1 << 12)
#define PS2_CLK_L   GPIOB->BRR  = (1 << 12)

// ═══════════════════════════════════════════════════════════════
// ESC PWM (TIM4: PB8=黄线转向, PB9=白线油门, 50Hz)
// ═══════════════════════════════════════════════════════════════

static void escInit() {
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    GPIOB->CRH &= ~(0xF << 16 | 0xF << 20);
    GPIOB->CRH |= (0xB << 16 | 0xB << 20);  // AF push-pull, 50MHz

    TIM4->PSC  = 71;
    TIM4->ARR  = 19999;
    TIM4->CCR3 = PWM_NEUTRAL;
    TIM4->CCR4 = PWM_NEUTRAL;

    TIM4->CCMR2 = (6 << 12) | (1 << 11) | (6 << 4) | (1 << 3);
    TIM4->CCER  = TIM_CCER_CC3E | TIM_CCER_CC4E;
    TIM4->CR1   = TIM_CR1_ARPE;
    TIM4->EGR   = TIM_EGR_UG;
    TIM4->CR1  |= TIM_CR1_CEN;

    Serial.print("[ESC] TIM4 50Hz: PB8(转向/黄) PB9(油门/白)\n");
}

static void escSet(uint16_t throttle, uint16_t steering) {
    if (throttle < PWM_MIN) throttle = PWM_MIN;
    if (throttle > PWM_MAX) throttle = PWM_MAX;
    if (steering < PWM_MIN) steering = PWM_MIN;
    if (steering > PWM_MAX) steering = PWM_MAX;
    TIM4->CCR3 = steering;  // PB8 → 黄线 (转向)
    TIM4->CCR4 = throttle;  // PB9 → 白线 (油门)
}

// ═══════════════════════════════════════════════════════════════
// PS2 手柄驱动 (bit-bang SPI, LSB first, CLK idle HIGH)
// ═══════════════════════════════════════════════════════════════

static void ps2Init() {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    // PB15: 下拉输入 (CNF=10, MODE=00) — 匹配 WHEELTEC GPIO_Mode_IPD
    GPIOB->CRH &= ~(0xF << 28);
    GPIOB->CRH |=  (0x8 << 28);
    GPIOB->ODR &= ~(1 << 15);   // 下拉

    // PB12(CLK), PB13(CS), PB14(CMD): 推挽输出 50MHz
    GPIOB->CRH &= ~(0xF << 16 | 0xF << 20 | 0xF << 24);
    GPIOB->CRH |=  (0x3 << 16 | 0x3 << 20 | 0x3 << 24);

    PS2_CS_H;
    PS2_CLK_H;
    PS2_CMD_H;
}

// 单字节 SPI 交换 (LSB first, 匹配 WHEELTEC PS2_Cmd 时序)
// CLK: HIGH→delay(5)→LOW→delay(5)→HIGH→sample
static uint8_t ps2Xfer(uint8_t tx) {
    uint8_t rx = 0;
    for (uint16_t ref = 0x01; ref < 0x0100; ref <<= 1) {
        if (ref & tx) PS2_CMD_H; else PS2_CMD_L;
        PS2_CLK_H;
        delayMicroseconds(5);
        PS2_CLK_L;
        delayMicroseconds(5);
        PS2_CLK_H;
        if (PS2_DAT) rx |= ref;
    }
    delayMicroseconds(16);  // WHEELTEC 字节间隔
    return rx;
}

// 短轮询 (WHEELTEC PS2_ShortPoll)
static void ps2ShortPoll() {
    PS2_CS_L;
    delayMicroseconds(16);
    ps2Xfer(0x01);
    ps2Xfer(0x42);
    ps2Xfer(0x00);
    PS2_CS_H;
    delayMicroseconds(16);
}

// 读取控制器状态 (WHEELTEC PS2_ReadData)
static bool ps2Poll(uint8_t* buf) {
    PS2_CS_L;
    buf[0] = ps2Xfer(0x01);  // PS2_Cmd(Comd[0])
    buf[1] = ps2Xfer(0x42);  // PS2_Cmd(Comd[1])
    bool ok = (buf[1] == 0x79 || buf[1] == 0x73);
    for (int i = 2; i < 9; i++)
        buf[i] = ps2Xfer(0x00);
    PS2_CS_H;
    return ok;
}

// 发送配置序列 (完全匹配 WHEELTEC PS2_SetInit)
static bool ps2Config() {
    for (int attempt = 0; attempt < 4; attempt++) {
        delay(10);

        // PS2_SetInit: 3x ShortPoll → EnterConfing → TurnOnAnalogMode → ExitConfing
        ps2ShortPoll();
        ps2ShortPoll();
        ps2ShortPoll();

        // PS2_EnterConfing: 0x01,0x43,0x00,0x01,0x00
        PS2_CS_L;
        ps2Xfer(0x01); ps2Xfer(0x43); ps2Xfer(0x00); ps2Xfer(0x01); ps2Xfer(0x00);
        PS2_CS_H;
        delayMicroseconds(16);

        // PS2_TurnOnAnalogMode: 0x01,0x44,0x00,0x01(analog),0x03(lock),0x00,0x00,0x00,0x00
        PS2_CS_L;
        ps2Xfer(0x01); ps2Xfer(0x44); ps2Xfer(0x00);
        ps2Xfer(0x01);  // analog ON
        ps2Xfer(0x03);  // lock mode
        ps2Xfer(0x00); ps2Xfer(0x00); ps2Xfer(0x00); ps2Xfer(0x00);
        PS2_CS_H;
        delayMicroseconds(16);

        // PS2_ExitConfing: 0x01,0x43,0x00,0x00,0x5A,0x5A,0x5A,0x5A,0x5A
        PS2_CS_L;
        delayMicroseconds(16);
        ps2Xfer(0x01); ps2Xfer(0x43); ps2Xfer(0x00); ps2Xfer(0x00);
        ps2Xfer(0x5A); ps2Xfer(0x5A); ps2Xfer(0x5A); ps2Xfer(0x5A); ps2Xfer(0x5A);
        PS2_CS_H;
        delayMicroseconds(16);

        // 验证
        uint8_t buf[9];
        if (ps2Poll(buf) && (buf[1] == 0x79 || buf[1] == 0x73))
            return true;
    }
    return false;
}

// 摇杆 → PWM 映射
// LY: 0=上, 128=中, 255=下  → 油门: 上=前进
// LX: 0=左, 128=中, 255=右  → 转向: 右=右转
static uint16_t joyToThrottle(uint8_t ly) {
    int off = 128 - (int)ly;                     // 上推 = 正值
    if (abs(off) <= JOY_DEADBAND) return PWM_NEUTRAL;
    int val = (int)PWM_NEUTRAL + off * (int)MAX_THROTTLE / 128;
    if (val < PWM_MIN) val = PWM_MIN;
    if (val > PWM_MAX) val = PWM_MAX;
    return (uint16_t)val;
}

static uint16_t joyToSteering(uint8_t lx) {
    int off = (int)lx - 128;                     // 右推 = 正值
    if (abs(off) <= JOY_DEADBAND) return PWM_NEUTRAL;
    int val = (int)PWM_NEUTRAL + off * (int)MAX_STEER / 128;
    if (val < (int)(PWM_NEUTRAL - MAX_STEER)) val = PWM_NEUTRAL - MAX_STEER;
    if (val > (int)(PWM_NEUTRAL + MAX_STEER)) val = PWM_NEUTRAL + MAX_STEER;
    return (uint16_t)val;
}

// ═══════════════════════════════════════════════════════════════
// 蜂鸣器 (PA3, 经跳线->S8050, active-HIGH)
// ═══════════════════════════════════════════════════════════════

static void beepInit() {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xF << 12)) | (0x3 << 12);  // PA3
    BEEP_OFF;
}
static void beep(int ms)  { BEEP_ON; delay(ms); BEEP_OFF; }
static void beepArm()     { beep(60); delay(60); beep(60); }
static void beepDisarm()  { beep(250); }

// ═══════════════════════════════════════════════════════════════

void setup() {
    beepInit();  // 最先初始化, 避免 PA3 浮空误响

    Serial.begin(115200);
    delay(100);

    Serial.println("\nSTM32 HC6060A PS2 Controller");

    pinMode(PIN_LED2, OUTPUT);
    digitalWrite(PIN_LED2, HIGH);

    escInit();
    Serial.print("ESC: wait "); Serial.print(ESC_INIT_DELAY); Serial.println("ms");

    ps2Init();
    Serial.print("PS2: ");
    Serial.println(ps2Config() ? "OK" : "not found");

    // 显式确保锁定状态 (防止静态变量初始化异常)
    g_motorArmed = false;
    g_escReady   = false;
    g_lastCmdMs  = millis();
    escSet(PWM_NEUTRAL, PWM_NEUTRAL);

    Serial.println("READY. Press START to arm.\n");
}

void loop() {
    // ─── 1. 检测 PS2 手柄并读取 ───
    static bool     ps2Ok     = false;
    static uint32_t lastPs2   = 0;
    uint32_t now = millis();

    if (now - lastPs2 >= 20) {  // 50Hz 读取
        lastPs2 = now;
        uint8_t buf[9];
        if (ps2Poll(buf)) {
            ps2Ok = true;

            uint8_t  buttons1 = buf[2];  // SELECT,L3,R3,START,UP,RIGHT,DOWN,LEFT
            uint8_t  buttons2 = buf[3];  // L2,R2,L1,R1,△,○,×,□
            uint8_t  rx = buf[4];
            uint8_t  ry = buf[5];
            uint8_t  lx = buf[6];
            uint8_t  ly = buf[7];
            g_ps2_lx = lx;
            g_ps2_ly = ly;

            // 控制器休眠检测: 数据连续 10s 完全不变 → 判定休眠
            static uint8_t  lastLx = 0, lastLy = 0, lastBt2 = 0;
            static uint16_t freezeCnt = 0;
            if (lx == lastLx && ly == lastLy && buttons2 == lastBt2) {
                freezeCnt++;
                if (freezeCnt > 500 && g_motorArmed) {  // 10s @ 50Hz
                    g_motorArmed = false;
                    escSet(PWM_NEUTRAL, PWM_NEUTRAL);
                    beepDisarm();
                    Serial.println("[PS2] 控制器休眠, 自动锁定!");
                }
            } else {
                lastLx = lx; lastLy = ly; lastBt2 = buttons2;
                freezeCnt = 0;
            }

            // START 按钮: 首次连接后跳过 3 次轮询, 防止静态变量初始化异常误触发
            static uint8_t  startupSkip = 3;
            static bool     lastStart   = true;
            if (startupSkip > 0) {
                startupSkip--;
                lastStart = (buttons2 & 0x08);  // 同步初始状态
            } else {
                bool startPressed = (buttons2 & 0x08);
                if (startPressed && !lastStart) {
                    g_motorArmed = !g_motorArmed;
                    freezeCnt = 0;
                    Serial.print("[PS2] 电机 ");
                    Serial.println(g_motorArmed ? "解锁 (ARMED)" : "锁定 (DISARMED)");
                    if (g_motorArmed) beepArm(); else { beepDisarm(); escSet(PWM_NEUTRAL, PWM_NEUTRAL); }
                }
                lastStart = startPressed;
            }

            // PS2 模式: 右摇杆上下(LX)=油门, 左摇杆左右(LY)=转向
            if (g_motorArmed && g_escReady) {
                uint16_t thr = joyToThrottle(lx);
                uint16_t str = joyToSteering(ly);
                escSet(thr, str);
                g_throttle = thr;
                g_steering = str;
                g_lastCmdMs = now;
            }
        } else {
            // 手柄读数失败 (接收器或遥控器断开)
            if (ps2Ok) {
                Serial.println("[PS2] 手柄断开");
                ps2Ok = false;
                g_motorArmed = false;
                escSet(PWM_NEUTRAL, PWM_NEUTRAL);
            }

            // 周期性重试 PS2 初始化 (每 3 秒)
            static uint32_t lastRetry = 0;
            if (!ps2Ok && now - lastRetry >= 3000) {
                lastRetry = now;
                Serial.print("[PS2] 尝试连接... ");
                if (ps2Config()) {
                    Serial.println("成功! (analog mode)");
                } else {
                    Serial.println("未检测到");
                }
            }
        }
    }

    // ─── 2. 命令超时 (无 PS2 时归中) ───
    if (!ps2Ok && g_escReady && millis() - g_lastCmdMs > CMD_TIMEOUT_MS)
        escSet(PWM_NEUTRAL, PWM_NEUTRAL);

    // ─── 4. ESC 自检计时 ───
    static bool escDelayDone = false;
    if (!escDelayDone && now >= ESC_INIT_DELAY) {
        escDelayDone = true;
        g_escReady   = true;
        g_lastCmdMs  = now;
        Serial.println("[ESC] 自检完成 — 请按 PS2 START 解锁");
    }

    // ─── 5. LED: 快闪=ARMED, 中闪=LOCKED, 慢闪=无PS2 ───
    static uint32_t lastLedMs;
    static bool     ledOn;
    uint32_t iv = !ps2Ok ? 500 : g_motorArmed ? 100 : 250;
    if (now - lastLedMs >= iv) { lastLedMs = now; ledOn = !ledOn; digitalWrite(PIN_LED2, ledOn ? LOW : HIGH); }

    // ─── 6. 状态 (5Hz) ───
    static uint32_t lastStat;
    if (now - lastStat >= 200) {
        lastStat = now;
        // 方向
        const char* dir;
        int t = (int)g_throttle, s = (int)g_steering;
        if      (t > 1520 && s > 1520) dir = "FWD+RGT";
        else if (t > 1520 && s < 1480) dir = "FWD+LFT";
        else if (t < 1480 && s > 1520) dir = "REV+RGT";
        else if (t < 1480 && s < 1480) dir = "REV+LFT";
        else if (t > 1520) dir = "FWD";
        else if (t < 1480) dir = "REV";
        else if (s > 1520) dir = "RGT";
        else if (s < 1480) dir = "LFT";
        else               dir = "STOP";

        Serial.print(ps2Ok ? (g_motorArmed ? "ARM" : "LCK") : "---");
        Serial.print(" thr="); Serial.print(g_throttle);
        Serial.print(" st="); Serial.print(g_steering);
        Serial.print(" "); Serial.println(dir);
    }
}
