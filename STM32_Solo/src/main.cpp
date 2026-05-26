/**
 * STM32F103C8T6 — Blink Test (L3 执行与安全层验证)
 *
 * 定制板 (非 Blue Pill):
 *   MCU: STM32F103C8T6 (Cortex-M3, 72MHz, 64KB Flash, 20KB SRAM)
 *   USB-UART: CH9102 (USART1: PA9=TX, PA10=RX)
 *   LED2: PA4 (蓝灯, active-LOW) — 500ms 闪烁
 *   BEEP: PA3 (蜂鸣器, active-LOW)
 *   LED1/LED3/LED4: 电源指示灯 (非 GPIO 控制)
 *   烧录: serial @ 115200, -dtr,rts,dtr,,,,
 *
 * 验证: 工具链正常, LED2 以 500ms 周期闪烁, USART1 输出启动信息。
 */

// generic 变体默认 Serial→USART2 (PA2/PA3), 覆盖为 USART1 (PA9/PA10)
#include <Arduino.h>
HardwareSerial SerialUSART1(PA10, PA9);
#define Serial SerialUSART1

constexpr uint8_t PIN_LED2 = PA4;   // 用户可编程蓝灯 (active-LOW)

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println("\n====================================");
    Serial.println("STM32F103C8T6 — L3 Safety Controller");
    Serial.println("====================================");
    Serial.print("MCU: STM32F103C8T6 @ 72MHz\n");
    Serial.print("Flash: 64KB | SRAM: 20KB\n");
    Serial.print("Bootloader: v2.2 | RDP: Unlocked\n");
    Serial.print("LED2: PA4 (blue, active-LOW)\n");
    Serial.print("BEEP: PA3 (active-LOW)\n");
    Serial.println("====================================\n");

    pinMode(PIN_LED2, OUTPUT);
    digitalWrite(PIN_LED2, HIGH);  // LED off (active-LOW)

    Serial.println("[INIT] Blink test started — PA4 LED2, 500ms cycle");
}

void loop() {
    static unsigned long lastMs = 0;
    static bool ledOn = false;
    unsigned long now = millis();

    if (now - lastMs >= 500) {
        lastMs = now;
        ledOn = !ledOn;
        digitalWrite(PIN_LED2, ledOn ? LOW : HIGH);  // active-LOW: LOW=ON

        Serial.print("[");
        Serial.print(now);
        Serial.print("] LED2 ");
        Serial.println(ledOn ? "ON" : "OFF");
    }
}
