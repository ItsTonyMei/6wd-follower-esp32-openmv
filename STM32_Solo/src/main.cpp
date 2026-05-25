/**
 * STM32F103CB — Blink Test (L3 执行与安全层验证)
 *
 * 验证工具链烧录正常: PC13 LED 以 500ms 周期闪烁。
 * 同时 USART1 输出启动信息到串口监视器 (PA9=TX, 115200 baud)。
 */

#include <Arduino.h>

void setup() {
    // USART1: PA9=TX, PA10=RX (CH9102 USB-UART)
    Serial.begin(115200);  // USART1: PA9=TX, PA10=RX → CH9102 USB-UART
    delay(100);

    Serial.println("\n====================================");
    Serial.println("STM32F103CB — L3 Safety Controller");
    Serial.println("====================================");
    Serial.print("MCU: STM32F103CBT6 @ 72MHz\n");
    Serial.print("Flash: 128KB | SRAM: 20KB\n");
    Serial.print("Bootloader: v2.2 | RDP: Unlocked\n");
    Serial.println("====================================\n");

    // PC13: 板载 LED (大多数 STM32F103 板的默认 LED 引脚)
    pinMode(PC13, OUTPUT);
    digitalWrite(PC13, HIGH);  // LED off (active-low on most boards)

    Serial.println("[INIT] Blink test started — PC13 LED, 500ms cycle");
}

void loop() {
    static unsigned long lastMs = 0;
    static bool ledOn = false;
    unsigned long now = millis();

    if (now - lastMs >= 500) {
        lastMs = now;
        ledOn = !ledOn;
        digitalWrite(PC13, ledOn ? LOW : HIGH);  // active-low

        Serial.print("[");
        Serial.print(millis());
        Serial.print("] LED ");
        Serial.println(ledOn ? "ON" : "OFF");
    }
}
