/**
 * STM32F103C8T6 — L3 执行与安全层 (双路独立无刷电调)
 *
 * 功能:
 *   1. PS2 手柄控制 (CN4: PB12-PB15) — 手动遥控, 优先于 ESP32/ESP8266
 *   2. USART3 (PB10=TX, PB11=RX) 接收 ESP32/ESP8266 MotorCmd 下行协议
 *   3. TIM3 CH3 (PB0) → 左电机 ESC, CH4 (PB1) → 右电机 ESC
 *   4. 50Hz PWM 输出 + 坦克混控 (tank-mix)
 *   5. 60s 无操作 → 自动锁定 (FAILSAFE) | 30s 数据冻结 → 休眠锁定
 *   6. LED2 (PA4, 蓝灯) 模式指示 + 蜂鸣器 (PA3, 经跳线, active-HIGH)
 *
 * 控制优先级: PS2 手柄 (已连接时) > ESP32/ESP8266 串口
 *
 * 定制板 (C06B):
 *   MCU: STM32F103C8T6 (Cortex-M3, 72MHz, 64KB Flash, 20KB SRAM)
 *   USB-UART: CH9102 via USART1 (PA9=TX, PA10=RX) — 烧录+调试
 *   PS2: CN4 6P (PB12=CLK, PB13=CS, PB14=CMD, PB15=DATA)
 *   L2: USART3 (PB10=TX, PB11=RX) ← ESP32 (Serial2) / ESP8266 (UART0 swapped)
 *   ESC: PB0=左电机(→H8), PB1=右电机(→H9) — 两路三相无刷电调
 *   烧录: PlatformIO serial @ 115200, -dtr,rts,dtr,,,,
 */

#include <Arduino.h>
#include "oled.h"

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║                        PWM 行程参数 (按电调实测标定)                       ║
// ╚══════════════════════════════════════════════════════════════════════════╝
// ZTW Seal G2 船用电调的中位识别点约在 1000-1275μs 区间,
// 标准 1500μs 会被电调判定为前进而非停止。

constexpr uint16_t PWM_NEUTRAL    = 1275;  // 中位=停止 (μs)
constexpr uint16_t PWM_MIN        = 650;   // 下限=最大后退 (μs)
constexpr uint16_t PWM_MAX        = 1900;  // 上限=最大前进 (μs)

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║                        摇杆灵敏度 (PS2 油门/转向)                         ║
// ╚══════════════════════════════════════════════════════════════════════════╝
// 摇杆满行程偏离中位的 PWM 偏移量 (μs)
// 值越大 = 越灵敏, 值越小 = 越柔和

constexpr int      THR_SENSITIVITY = 600;   // 油门灵敏度 (满杆=中位±600μs)
constexpr int      STR_SENSITIVITY = 400;   // 转向灵敏度 (满杆=中位±400μs)
constexpr int      JOY_DEADBAND    = 5;     // 摇杆中位死区 (PS2值±5≈PWM±20μs)

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║                           时序与安全参数                                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝

constexpr uint32_t ESC_INIT_DELAY    = 3000;   // ESC 上电自检时间 (ms)
constexpr uint32_t CMD_TIMEOUT_MS    = 60000;  // 命令超时: 无操作多久自动锁定 (60s)
constexpr uint32_t PS2_RETRY_MS      = 3000;   // PS2 断连后重试间隔 (ms)
constexpr uint32_t SLEEP_FREEZE_CNT  = 1500;   // PS2 数据冻结判定休眠 (1500×20ms=30s)
constexpr uint32_t STATUS_INTERVAL   = 200;    // 串口/OLED 状态刷新间隔 (ms)
constexpr int      DIR_THRESHOLD     = 20;     // 方向判定阈值 (偏离中位多少算前进/后退, μs)

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║                           硬件引脚定义                                    ║
// ╚══════════════════════════════════════════════════════════════════════════╝

constexpr uint8_t PIN_LED2 = PA4;              // LED2 蓝色, active-LOW

// PA3 蜂鸣器 (经跳线→S8050 驱动, active-HIGH: PA3=1 响)
#define BEEP_ON   GPIOA->BSRR = (1 << 3)
#define BEEP_OFF  GPIOA->BRR  = (1 << 3)

// USART1: USB 调试 (CH9102)
HardwareSerial SerialUSART1(PA10, PA9);
#define Serial SerialUSART1

// USART3: ESP32/ESP8266 通信
HardwareSerial SerialESP(PB11, PB10);           // RX=PB11←L2 TX, TX=PB10→L2 RX

// ─── PS2 引脚宏 (C06B CN4: PB15=DATA, PB14=CMD, PB13=CS, PB12=CLK) ───
#define PS2_DAT     ((GPIOB->IDR >> 15) & 1)
#define PS2_CMD_H   GPIOB->BSRR = (1 << 14)
#define PS2_CMD_L   GPIOB->BRR  = (1 << 14)
#define PS2_CS_H    GPIOB->BSRR = (1 << 13)
#define PS2_CS_L    GPIOB->BRR  = (1 << 13)
#define PS2_CLK_H   GPIOB->BSRR = (1 << 12)
#define PS2_CLK_L   GPIOB->BRR  = (1 << 12)

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║                           全局状态变量                                    ║
// ╚══════════════════════════════════════════════════════════════════════════╝

static uint16_t  g_throttle   = PWM_NEUTRAL;
static uint16_t  g_steering   = PWM_NEUTRAL;
static uint32_t  g_lastCmdMs  = 0;
static bool      g_escReady   = false;
static bool      g_motorArmed = false;
static bool      g_espMode    = false;          // false=PS2, true=ESP32/ESP8266
static uint8_t   g_ps2_lx     = 128;
static uint8_t   g_ps2_ly     = 128;

// ═══════════════════════════════════════════════════════════════
// CRC8 (poly 0x07, init 0x00 — 与 ESP32/ESP8266 一致)
// ═══════════════════════════════════════════════════════════════

static uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : crc << 1;
    }
    return crc;
}

// ═══════════════════════════════════════════════════════════════
// ESC PWM (TIM3: PB0=左电机, PB1=右电机, 50Hz)
// ═══════════════════════════════════════════════════════════════

static void escInit() {
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    GPIOB->CRL &= ~(0xF << 0 | 0xF << 4);        // 清除 PB0, PB1
    GPIOB->CRL |= (0xB << 0 | 0xB << 4);         // PB0/PB1: AF push-pull, 50MHz

    TIM3->PSC  = 71;
    TIM3->ARR  = 19999;
    TIM3->CCR3 = PWM_NEUTRAL;
    TIM3->CCR4 = PWM_NEUTRAL;

    TIM3->CCMR2 = (6 << 12) | (1 << 11) | (6 << 4) | (1 << 3);
    TIM3->CCER  = TIM_CCER_CC3E | TIM_CCER_CC4E;
    TIM3->CR1   = TIM_CR1_ARPE;
    TIM3->EGR   = TIM_EGR_UG;
    TIM3->CR1  |= TIM_CR1_CEN;

    Serial.print("[ESC] TIM3 50Hz: PB0(左) PB1(右)\n");
}

// 坦克混控: throttle+steering → 左/右独立 PWM
static void escSet(uint16_t throttle, uint16_t steering) {
    int sOff = (int)steering - (int)PWM_NEUTRAL;
    int left  = (int)throttle + sOff;
    int right = (int)throttle - sOff;
    if (left  < PWM_MIN) left  = PWM_MIN;
    if (left  > PWM_MAX) left  = PWM_MAX;
    if (right < PWM_MIN) right = PWM_MIN;
    if (right > PWM_MAX) right = PWM_MAX;
    TIM3->CCR3 = (uint16_t)left;                 // PB0 → 左电机
    TIM3->CCR4 = (uint16_t)right;                // PB1 → 右电机
}

// ═══════════════════════════════════════════════════════════════
// PS2 手柄驱动 (bit-bang SPI, LSB first, CLK idle HIGH)
// ═══════════════════════════════════════════════════════════════

static void ps2Init() {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    // PB15: 下拉输入 (CNF=10, MODE=00) — 匹配 WHEELTEC GPIO_Mode_IPD
    GPIOB->CRH &= ~(0xF << 28);
    GPIOB->CRH |=  (0x8 << 28);
    GPIOB->ODR &= ~(1 << 15);

    // PB12(CLK), PB13(CS), PB14(CMD): 推挽输出 50MHz
    GPIOB->CRH &= ~(0xF << 16 | 0xF << 20 | 0xF << 24);
    GPIOB->CRH |=  (0x3 << 16 | 0x3 << 20 | 0x3 << 24);

    PS2_CS_H;
    PS2_CLK_H;
    PS2_CMD_H;
}

static uint8_t ps2Xfer(uint8_t tx) {
    uint8_t rx = 0;
    for (uint16_t ref = 0x01; ref < 0x0100; ref <<= 1) {
        if (ref & tx) PS2_CMD_H; else PS2_CMD_L;
        PS2_CLK_H; delayMicroseconds(5);
        PS2_CLK_L; delayMicroseconds(5);
        PS2_CLK_H;
        if (PS2_DAT) rx |= ref;
    }
    delayMicroseconds(16);
    return rx;
}

static void ps2ShortPoll() {
    PS2_CS_L; delayMicroseconds(16);
    ps2Xfer(0x01); ps2Xfer(0x42); ps2Xfer(0x00);
    PS2_CS_H; delayMicroseconds(16);
}

static bool ps2Poll(uint8_t* buf) {
    PS2_CS_L;
    buf[0] = ps2Xfer(0x01);
    buf[1] = ps2Xfer(0x42);
    bool ok = (buf[1] == 0x79 || buf[1] == 0x73);
    for (int i = 2; i < 9; i++) buf[i] = ps2Xfer(0x00);
    PS2_CS_H;
    return ok;
}

static bool ps2Config() {
    for (int attempt = 0; attempt < 4; attempt++) {
        delay(10);
        ps2ShortPoll(); ps2ShortPoll(); ps2ShortPoll();

        // EnterConfing
        PS2_CS_L;
        ps2Xfer(0x01); ps2Xfer(0x43); ps2Xfer(0x00); ps2Xfer(0x01); ps2Xfer(0x00);
        PS2_CS_H; delayMicroseconds(16);

        // TurnOnAnalogMode
        PS2_CS_L;
        ps2Xfer(0x01); ps2Xfer(0x44); ps2Xfer(0x00);
        ps2Xfer(0x01); ps2Xfer(0x03);
        ps2Xfer(0x00); ps2Xfer(0x00); ps2Xfer(0x00); ps2Xfer(0x00);
        PS2_CS_H; delayMicroseconds(16);

        // ExitConfing
        PS2_CS_L; delayMicroseconds(16);
        ps2Xfer(0x01); ps2Xfer(0x43); ps2Xfer(0x00); ps2Xfer(0x00);
        ps2Xfer(0x5A); ps2Xfer(0x5A); ps2Xfer(0x5A); ps2Xfer(0x5A); ps2Xfer(0x5A);
        PS2_CS_H; delayMicroseconds(16);

        uint8_t buf[9];
        if (ps2Poll(buf) && (buf[1] == 0x79 || buf[1] == 0x73)) return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════
// 摇杆 → PWM 映射
// ═══════════════════════════════════════════════════════════════

static uint16_t joyToThrottle(uint8_t ly) {
    int off = 128 - (int)ly;                     // 上推=正值
    if (abs(off) <= JOY_DEADBAND) return PWM_NEUTRAL;
    int val = (int)PWM_NEUTRAL + off * THR_SENSITIVITY / 128;
    if (val < PWM_MIN) val = PWM_MIN;
    if (val > PWM_MAX) val = PWM_MAX;
    return (uint16_t)val;
}

static uint16_t joyToSteering(uint8_t lx) {
    int off = (int)lx - 128;                     // 右推=正值
    if (abs(off) <= JOY_DEADBAND) return PWM_NEUTRAL;
    int val = (int)PWM_NEUTRAL + off * STR_SENSITIVITY / 128;
    if (val < PWM_MIN) val = PWM_MIN;
    if (val > PWM_MAX) val = PWM_MAX;
    return (uint16_t)val;
}

// ═══════════════════════════════════════════════════════════════
// 蜂鸣器 (PA3, 经跳线->S8050, active-HIGH)
// ═══════════════════════════════════════════════════════════════

static void beepInit() {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xF << 12)) | (0x3 << 12);
    BEEP_OFF;
}
static void beep(int ms)  { BEEP_ON; delay(ms); BEEP_OFF; }
static void beepArm()     { beep(60); delay(60); beep(60); }
static void beepDisarm()  { beep(250); }

// ═══════════════════════════════════════════════════════════════

void setup() {
    beepInit();

    Serial.begin(115200);
    SerialESP.begin(115200);
    delay(100);

    Serial.println("\nSTM32 Dual BLDC Tracked Controller");
    Serial.println("[USART3] PB11(RX) PB10(TX) @ 115200 baud ← L2 控制器");

    pinMode(PIN_LED2, OUTPUT);
    digitalWrite(PIN_LED2, HIGH);

    escInit();
    Serial.print("[ESC] TIM3 50Hz: PB0(左) PB1(右) — 上电输出中位, ");

    ps2Init();
    Serial.print("PS2: ");
    Serial.println(ps2Config() ? "OK" : "not found");

    oledInit();

    g_motorArmed = false;
    g_espMode    = false;
    g_escReady   = false;
    g_lastCmdMs  = millis();
    escSet(PWM_NEUTRAL, PWM_NEUTRAL);

    Serial.println("READY. 3s 后 PWM 跟随摇杆.\n");
}

void loop() {
    // ─── 1. 检测 PS2 手柄并读取 (50Hz) ───
    static bool     ps2Ok       = false;
    static uint32_t lastPs2     = 0;
    static uint8_t  startupSkip = 3;
    uint32_t now = millis();

    if (now - lastPs2 >= 20) {
        lastPs2 = now;
        uint8_t buf[9];
        if (ps2Poll(buf)) {
            ps2Ok = true;

            uint8_t  buttons1 = buf[2];          // SELECT,L3,R3,START,UP,RIGHT,DOWN,LEFT
            uint8_t  buttons2 = buf[3];          // L2,R2,L1,R1,△,○,×,□
            uint8_t  rx = buf[4], ry = buf[5];
            uint8_t  lx = buf[6], ly = buf[7];
            g_ps2_lx = lx;
            g_ps2_ly = ly;

            // 控制器休眠检测
            static uint8_t  lastLx = 0, lastLy = 0, lastBt2 = 0;
            static uint16_t freezeCnt = 0;
            if (lx == lastLx && ly == lastLy && buttons2 == lastBt2) {
                freezeCnt++;
                if (freezeCnt > SLEEP_FREEZE_CNT && g_motorArmed && !g_espMode) {
                    g_motorArmed = false;
                    escSet(PWM_NEUTRAL, PWM_NEUTRAL);
                    beepDisarm();
                    Serial.println("[PS2] 控制器休眠, 自动锁定!");
                }
            } else {
                lastLx = lx; lastLy = ly; lastBt2 = buttons2;
                freezeCnt = 0;
            }

            // START / SELECT 按钮
            static bool lastStart = true, lastSel = true;
            if (startupSkip > 0) {
                startupSkip--;
                lastStart = (buttons2 & 0x08);
                lastSel   = (buttons2 & 0x01);
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

            // SELECT 切换控制源 (buttons2 bit0, 低有效, 松开触发)
            bool selRel = (buttons2 & 0x01);
            if (selRel && !lastSel) {
                g_espMode = !g_espMode;
                if (g_motorArmed) { g_motorArmed = false; escSet(PWM_NEUTRAL, PWM_NEUTRAL); }
                beepDisarm();
                Serial.print("[PS2] 切换到 ");
                Serial.println(g_espMode ? "ESP 模式" : "PS2 手柄模式");
            }
            lastSel = selRel;

            // PS2 模式: 摇杆→油门/转向→坦克混控→左/右 PWM (需 ARMED)
            // 仅摇杆实际移动时刷新超时计时器 (静止不动 20s → 自动锁定)
            uint16_t thr = joyToThrottle(lx);
            uint16_t str = joyToSteering(ly);
            static uint8_t lastArmedLx = 128, lastArmedLy = 128;
            if (g_motorArmed && g_escReady && !g_espMode) {
                escSet(thr, str);
                g_throttle = thr;
                g_steering = str;
                if (lx != lastArmedLx || ly != lastArmedLy)
                    g_lastCmdMs = now;
                lastArmedLx = lx;
                lastArmedLy = ly;
            } else {
                lastArmedLx = 128; lastArmedLy = 128;
            }
        } else {
            if (ps2Ok) {
                Serial.println("[PS2] 手柄断开");
                ps2Ok = false;
                g_motorArmed = false;
                startupSkip = 3;
                escSet(PWM_NEUTRAL, PWM_NEUTRAL);
            }

            static uint32_t lastRetry = 0;
            if (!ps2Ok && now - lastRetry >= PS2_RETRY_MS) {
                lastRetry = now;
                Serial.print("[PS2] 尝试连接... ");
                if (ps2Config()) {
                    Serial.println("成功!");
                    startupSkip = 3;
                } else {
                    Serial.println("未检测到");
                }
            }
        }
    }

    // ─── 2. L2 串口接收 (USART3, PB11=RX ← ESP32/ESP8266 TX) ───
    while (SerialESP.available() >= 6) {
        if (SerialESP.read() != 0xAA) continue;

        uint8_t buf[5];
        bool ok = true;
        for (int i = 0; i < 5; i++) {
            uint32_t t0 = micros();
            while (!SerialESP.available()) {
                if (micros() - t0 > 2000) { ok = false; break; }
            }
            if (!ok) break;
            buf[i] = SerialESP.read();
        }
        if (!ok) break;

        if (buf[4] != crc8(buf, 4)) continue;

        uint16_t thr = buf[0] | ((uint16_t)buf[1] << 8);
        uint16_t str = buf[2] | ((uint16_t)buf[3] << 8);
        if (thr < PWM_MIN || thr > PWM_MAX || str < PWM_MIN || str > PWM_MAX) continue;

        if (g_espMode && g_motorArmed && g_escReady) {
            escSet(thr, str);
            g_throttle = thr;
            g_steering = str;
            g_lastCmdMs = millis();
        }
    }

    // ─── 3. 命令超时 (20s 无有效命令 → 自动锁定+蜂鸣) ───
    if (g_escReady && g_motorArmed && millis() - g_lastCmdMs > CMD_TIMEOUT_MS) {
        g_motorArmed = false;
        escSet(PWM_NEUTRAL, PWM_NEUTRAL);
        beepDisarm();
        Serial.println("[SAFE] 命令超时, 自动锁定!");
    }

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

    // ─── 6. 状态输出 (5Hz) ───
    static uint32_t lastStat;
    if (now - lastStat >= STATUS_INTERVAL) {
        lastStat = now;

        // 方向判定
        const char* dir;
        int t = (int)g_throttle, s = (int)g_steering;
        int hi = (int)PWM_NEUTRAL + DIR_THRESHOLD;
        int lo = (int)PWM_NEUTRAL - DIR_THRESHOLD;
        if      (t > hi && s > hi) dir = "FWD+RGT";
        else if (t > hi && s < lo) dir = "FWD+LFT";
        else if (t < lo && s > hi) dir = "REV+RGT";
        else if (t < lo && s < lo) dir = "REV+LFT";
        else if (t > hi) dir = "FWD";
        else if (t < lo) dir = "REV";
        else if (s > hi) dir = "RGT";
        else if (s < lo) dir = "LFT";
        else              dir = "STOP";

        // 坦克混控后左/右 PWM
        int sOff = s - (int)PWM_NEUTRAL;
        int left  = t + sOff; if (left  < (int)PWM_MIN) left = PWM_MIN; if (left > (int)PWM_MAX) left = PWM_MAX;
        int right = t - sOff; if (right < (int)PWM_MIN) right = PWM_MIN; if (right > (int)PWM_MAX) right = PWM_MAX;

        Serial.print(g_espMode ? "ESP" : "PS2");
        Serial.print(ps2Ok ? (g_motorArmed ? " ARM" : " LCK") : " ---");
        Serial.print(" thr="); Serial.print(g_throttle);
        Serial.print(" st="); Serial.print(g_steering);
        Serial.print(" L="); Serial.print(left);
        Serial.print(" R="); Serial.print(right);
        Serial.print(" "); Serial.println(dir);

        oledClear();
        char oledBuf[20];
        snprintf(oledBuf, sizeof(oledBuf), "%s %s", g_espMode ? "ESP" : "PS2",
                 ps2Ok ? (g_motorArmed ? "ARM" : "LCK") : "---");
        oledShowString(0, 0, oledBuf);
        snprintf(oledBuf, sizeof(oledBuf), "T:%4u S:%4u", g_throttle, g_steering);
        oledShowString(0, 16, oledBuf);
        snprintf(oledBuf, sizeof(oledBuf), "L:%4d R:%4d", left, right);
        oledShowString(0, 32, oledBuf);
        snprintf(oledBuf, sizeof(oledBuf), "%s", dir);
        oledShowString(0, 48, oledBuf);
        oledRefresh();
    }
}
