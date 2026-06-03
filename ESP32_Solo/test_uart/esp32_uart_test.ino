// ============================================================================
// ESP32 UART 基础功能测试 — UART0 (Serial) + UART2 (Serial2) 回环测试
// 硬件: ESP32-WROOM-32U DevKit V1
// ============================================================================
// 接线要求:
//   - USB 连接 PC → UART0 (TX0=GPIO1, RX0=GPIO3) 经 CH340/CP2102
//   - UART2 回环测试: 用杜邦线短接 GPIO16(RX2) ↔ GPIO17(TX2)
// ============================================================================

#include <Arduino.h>

// UART2 引脚 (与 Config.h 一致)
#define PIN_UART2_RX  16
#define PIN_UART2_TX  17
#define UART2_BAUD    115200

// 测试参数
#define TEST_INTERVAL_MS     2000   // 每 2 秒打印一次状态
#define LOOPBACK_PATTERN_LEN  8     // 回环测试数据长度

static unsigned long lastReportMs = 0;
static uint32_t      loopCount    = 0;

// ─── UART0 RX 测试: 回显接收到的数据 ───
static bool uart0RxTest() {
  bool gotData = false;
  while (Serial.available()) {
    char c = (char)Serial.read();
    Serial.print("[UART0_RX echo: 0x");
    if (c < 0x10) Serial.print('0');
    Serial.print(c, HEX);
    Serial.print("='");
    Serial.print(c);
    Serial.println("']");
    gotData = true;
  }
  return gotData;
}

// ─── UART2 回环测试: TX2→发, RX2→收, 比对 ───
static void uart2LoopbackTest() {
  // 清空接收缓冲区
  while (Serial2.available()) { Serial2.read(); }

  // 生成递增测试 pattern
  uint8_t txBuf[LOOPBACK_PATTERN_LEN];
  uint32_t seq = loopCount;
  txBuf[0] = 0xAA;                           // 帧头
  txBuf[1] = 0x55;                           // 帧头2
  txBuf[2] = (uint8_t)(seq & 0xFF);          // 序号低字节
  txBuf[3] = (uint8_t)((seq >> 8) & 0xFF);   // 序号高字节
  txBuf[4] = (uint8_t)((seq >> 16) & 0xFF);
  txBuf[5] = (uint8_t)((seq >> 24) & 0xFF);
  // XOR checksum
  uint8_t csum = 0;
  for (int i = 0; i < 6; i++) csum ^= txBuf[i];
  txBuf[6] = csum;
  txBuf[7] = 0xBB;  // 帧尾

  // 发送
  size_t sent = Serial2.write(txBuf, LOOPBACK_PATTERN_LEN);
  Serial2.flush();

  // 等待回环数据 (最多 100ms)
  uint32_t t0 = millis();
  uint8_t rxBuf[LOOPBACK_PATTERN_LEN] = {0};
  size_t  rxIdx = 0;

  while (millis() - t0 < 200) {
    if (Serial2.available()) {
      rxBuf[rxIdx++] = (uint8_t)Serial2.read();
      if (rxIdx >= LOOPBACK_PATTERN_LEN) break;
    }
  }

  // 比对结果
  bool match = true;
  for (size_t i = 0; i < LOOPBACK_PATTERN_LEN; i++) {
    if (rxBuf[i] != txBuf[i]) { match = false; break; }
  }

  // 打印测试结果
  Serial.print("  TX2 → [");
  for (size_t i = 0; i < LOOPBACK_PATTERN_LEN; i++) {
    if (txBuf[i] < 0x10) Serial.print('0');
    Serial.print(txBuf[i], HEX);
    Serial.print(' ');
  }
  Serial.println("]");

  Serial.print("  RX2 ← [");
  for (size_t i = 0; i < LOOPBACK_PATTERN_LEN; i++) {
    if (rxBuf[i] < 0x10) Serial.print('0');
    Serial.print(rxBuf[i], HEX);
    Serial.print(' ');
  }
  Serial.println("]");

  if (match) {
    Serial.println("  ✅ [UART2] Loopback PASSED — data matches");
  } else {
    if (rxIdx == 0) {
      Serial.println("  ⚠️  [UART2] Loopback FAILED — no data received (check GPIO16↔GPIO17 jumper)");
    } else {
      Serial.print("  ❌ [UART2] Loopback FAILED — ");
      Serial.print(rxIdx);
      Serial.print(" bytes received, data mismatch (expected ");
      Serial.print(LOOPBACK_PATTERN_LEN);
      Serial.println(" bytes)");
    }
  }
}

void setup() {
  // ─── UART0: USB Serial (GPIO1=TX, GPIO3=RX) ───
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║   ESP32 UART0 + UART2 Test              ║");
  Serial.println("╠══════════════════════════════════════════╣");
  Serial.println("║ UART0 (Serial):  GPIO1(TX) GPIO3(RX)    ║");
  Serial.println("║   → USB-Serial CH340/CP2102 → PC        ║");
  Serial.println("║   Baud: 115200                          ║");
  Serial.println("╠══════════════════════════════════════════╣");
  Serial.println("║ UART2 (Serial2): GPIO17(TX) GPIO16(RX)  ║");
  Serial.println("║   Baud: 115200                          ║");
  Serial.println("║   ⚡ 回环测试: 需用杜邦线短接 TX2↔RX2     ║");
  Serial.println("╠══════════════════════════════════════════╣");
  Serial.println("║ 串口监视器输入任意字符 → UART0 RX 测试    ║");
  Serial.println("╚══════════════════════════════════════════╝");
  Serial.println();

  // ─── UART2: 初始化 ───
  Serial.print("[INIT] Starting Serial2 on GPIO");
  Serial.print(PIN_UART2_TX);
  Serial.print("(TX) / GPIO");
  Serial.print(PIN_UART2_RX);
  Serial.print("(RX) @ ");
  Serial.print(UART2_BAUD);
  Serial.println(" baud...");

  Serial2.begin(UART2_BAUD, SERIAL_8N1, PIN_UART2_RX, PIN_UART2_TX);

  Serial.println("[INIT] Serial2 initialized OK");
  Serial.println();
  Serial.println(">>> 等待回环跳线: GPIO16(RX2) ↔ GPIO17(TX2) <<<");
  Serial.println();
  Serial.println("--- Running ---");
}

void loop() {
  loopCount++;

  // 每 2 秒输出一次测试结果
  if (millis() - lastReportMs >= TEST_INTERVAL_MS) {
    lastReportMs = millis();

    Serial.println();
    Serial.print("══════ Loop #");
    Serial.print(loopCount);
    Serial.print(" (UART2 data seq=0x");
    Serial.print(loopCount, HEX);
    Serial.println(") ══════");

    // 1) UART0 TX 测试 → 这些打印本身就是证明
    Serial.println("[UART0_TX] ✅ PASSED — you see this line");

    // 2) UART2 回环测试
    uart2LoopbackTest();

    // 3) UART0 RX 提示
    Serial.println("[UART0_RX] Type any character in Serial Monitor to test RX0...");
  }

  // 持续检查 UART0 RX
  uart0RxTest();

  delay(10);
}
