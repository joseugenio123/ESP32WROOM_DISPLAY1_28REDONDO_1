#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

const uint8_t fontDigits[10][5] = {
  {0x7F, 0x02, 0x04, 0x08, 0x7F}, // N (neutral)
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

uint16_t colors[6] = {TFT_RED, TFT_BLUE, TFT_BLUE, TFT_BLUE, TFT_BLUE, TFT_BLUE};
uint16_t centerColors[6] = {TFT_RED, TFT_ORANGE, TFT_ORANGE, TFT_ORANGE, TFT_ORANGE, TFT_GREEN};

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

void slideSet(int oldNum, int currNum, int nextNum,
              uint16_t oldColor, uint16_t currColor, uint16_t nextColor) {
  int steps = 5;
  int distance = 80;
  for (int i = 0; i <= steps; i++) {
    tft.fillScreen(TFT_BLACK);
    int offset = (distance * i) / steps;
    drawNumber(110, 106 - offset, oldNum, 4, oldColor);
    drawNumber(95, 85 + distance - offset, currNum, 10, currColor);
    if (nextNum != -1) {
      drawNumber(110, 106 + 2 * distance - offset, nextNum, 4, nextColor);
    }
    delay(10);
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
  tft.fillScreen(TFT_BLACK);
  drawNumber(95, 85, 0, 10, centerColors[0]);
  drawNumber(110, 186, 1, 4, colors[1]);
  tft.drawCircle(120, 120, 55, TFT_ORANGE);
  delay(1500);

  for (int n = 0; n <= 3; n++) {
    slideSet(n, n + 1, n + 2, colors[n], centerColors[n + 1], colors[n + 2]);
    tft.drawCircle(120, 120, 55, TFT_ORANGE);
    delay(1500);
  }

  slideSet(4, 5, -1, colors[4], centerColors[5], 0);
  tft.drawCircle(120, 120, 55, TFT_ORANGE);
  delay(1500);
}
