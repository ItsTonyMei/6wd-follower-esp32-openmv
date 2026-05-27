// ============================================================================
// ESP8266 ↔ STM32 通信测试
// 接线:
//   ESP8266 D6 (GPIO12) → STM32 PA10 (RX)
//   ESP8266 D7 (GPIO13) → STM32 PA9  (TX)
//   ESP8266 GND         → STM32 GND
// ============================================================================

#include <SoftwareSerial.h>

// STM32 通信口 (交叉连接)
SoftwareSerial stm32Serial(13, 12, false);  // RX=D7(GPIO13), TX=D6(GPIO12), no inverse

// 测试参数
const unsigned long PING_INTERVAL = 2000;  // 每 2 秒发一次 PING
unsigned long lastPing = 0;
unsigned int  pingCount = 0;
unsigned int  pongCount = 0;
unsigned long lastPongMs = 0;

// ─── Setup ───
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n========================================");
    Serial.println("  ESP8266 ↔ STM32 通信测试");
    Serial.println("========================================");
    Serial.println("[接线] D6(GPIO12) → STM32 PA10(RX)");
    Serial.println("[接线] D7(GPIO13) → STM32 PA9 (TX)");
    Serial.println("[接线] GND        → STM32 GND");
    Serial.println("[协议] 115200 baud, 8N1");
    Serial.println("========================================\n");

    stm32Serial.begin(115200);

    // 发第一条握手消息
    stm32Serial.println("HELLO_STM32");
    Serial.println("[ESP→STM] HELLO_STM32");
}

// ─── Loop ───
void loop() {
    // ── STM32 → ESP8266 接收 ──
    while (stm32Serial.available()) {
        String line = stm32Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            lastPongMs = millis();
            pongCount++;
            Serial.print("[STM→ESP] \"");
            Serial.print(line);
            Serial.print("\"  (总收到 ");
            Serial.print(pongCount);
            Serial.println(" 条)");

            // 检查 PONG 回应中的序号
            if (line.startsWith("PONG")) {
                int idx = line.lastIndexOf(' ');
                if (idx > 0) {
                    int rspCount = line.substring(idx + 1).toInt();
                    Serial.print("  ↳ 回应 PING #");
                    Serial.println(rspCount);
                }
            }
        }
    }

    // ── ESP8266 → STM32 发送 (每 2 秒 PING) ──
    unsigned long now = millis();
    if (now - lastPing >= PING_INTERVAL) {
        lastPing = now;
        pingCount++;
        String ping = "PING " + String(pingCount);
        stm32Serial.println(ping);
        Serial.print("[ESP→STM] \"");
        Serial.print(ping);
        Serial.println("\"");

        // 检查超时 (上一轮 PING 未收到 PONG)
        if (pongCount > 0 && (now - lastPongMs > PING_INTERVAL * 2)) {
            Serial.print("  ⚠ 超时: 上次收到 PONG 是 ");
            Serial.print((now - lastPongMs) / 1000);
            Serial.println("s 前");
        }
    }

    // ── 统计输出 (每 10 秒) ──
    static unsigned long lastStats = 0;
    if (now - lastStats >= 10000) {
        lastStats = now;
        float lossRate = pingCount > 0 ? (1.0 - (float)pongCount / pingCount) * 100 : 0;
        Serial.println("---");
        Serial.print("[统计] 发送: ");
        Serial.print(pingCount);
        Serial.print(" PING | 收到: ");
        Serial.print(pongCount);
        Serial.print(" PONG | 丢包率: ");
        Serial.print(lossRate, 1);
        Serial.println("%");
        Serial.println("---");
    }
}
