#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

const uint8_t fontDigits[10][5] = {
  {0x3E, 0x51, 0x49, 0x45, 0x3E},
  {0x00, 0x42, 0x7F, 0x40, 0x00},
  {0x42, 0x61, 0x51, 0x49, 0x46},
  {0x21, 0x41, 0x45, 0x4B, 0x31},
  {0x18, 0x14, 0x12, 0x7F, 0x10},
  {0x27, 0x45, 0x45, 0x45, 0x39},
  {0x3C, 0x4A, 0x49, 0x49, 0x30},
  {0x01, 0x71, 0x09, 0x05, 0x03},
  {0x36, 0x49, 0x49, 0x49, 0x36},
  {0x06, 0x49, 0x49, 0x29, 0x1E},
};

uint16_t colors[5] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_CYAN};

void drawNumber(int x, int y, uint8_t num, uint8_t size, uint16_t color) {
  if (num > 9) return;
  for (int col = 0; col < 5; col++) {
    uint8_t line = fontDigits[num][col];
    for (int row = 0; row < 7; row++) {
      if (line & (1 << row)) {
        tft.fillRect(x + col * size, y + row * size, size, size, color);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
}

void loop() {
  for (int n = 1; n <= 5; n++) {
    tft.fillScreen(TFT_BLACK);
    drawNumber(95, 85, n, 10, colors[n-1]);
    delay(1500);
  }
}
