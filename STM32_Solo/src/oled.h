#pragma once
#include <Arduino.h>

// ═══ OLED 128x64 (SH1106/SSD1306) 4线SPI ═══
// C06B H2: PB3=RST, PB4=SDIN, PB5=SCLK, PA15=DC

void oledInit();
void oledClear();
void oledShowString(uint8_t x, uint8_t y, const char* str);
void oledShowNumber(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size);
void oledRefresh();
