#include <Arduino.h>
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel;
  lgfx::Bus_SPI _bus;
public:
  LGFX() {
    auto cfg = _bus.config();
    cfg.spi_host = SPI2_HOST;
    cfg.spi_mode = 0;
    cfg.freq_write = 27000000;
    cfg.freq_read = 16000000;
    cfg.pin_sclk = 18;
    cfg.pin_mosi = 23;
    cfg.pin_miso = -1;
    cfg.pin_dc = 16;
    _bus.config(cfg);
    _panel.setBus(&_bus);

    auto pcfg = _panel.config();
    pcfg.pin_cs = 5;
    pcfg.pin_rst = 17;
    pcfg.pin_busy = -1;
    pcfg.memory_width = 240;
    pcfg.memory_height = 240;
    pcfg.panel_width = 240;
    pcfg.panel_height = 240;
    pcfg.offset_x = 0;
    pcfg.offset_y = 0;
    pcfg.offset_rotation = 0;
    pcfg.readable = false;
    pcfg.invert = true;
    pcfg.rgb_order = false;
    pcfg.dlen_16bit = false;
    pcfg.bus_shared = true;
    _panel.config(pcfg);

    setPanel(&_panel);
  }
};

LGFX tft;

const uint8_t fontDigits[10][5] = {
  {0x7F, 0x02, 0x04, 0x08, 0x7F},
  {0x00, 0x42, 0x7F, 0x40, 0x00},
  {0x42, 0x61, 0x51, 0x49, 0x46},
  {0x21, 0x41, 0x45, 0x4B, 0x31},
  {0x18, 0x14, 0x12, 0x7F, 0x10},
  {0x27, 0x45, 0x45, 0x45, 0x39},
  {0x08, 0x08, 0x08, 0x08, 0x08},
  {0x01, 0x71, 0x09, 0x05, 0x03},
  {0x36, 0x49, 0x49, 0x49, 0x36},
  {0x06, 0x49, 0x49, 0x29, 0x1E},
};

uint16_t colors[6] = {TFT_RED, TFT_BLUE, TFT_BLUE, TFT_BLUE, TFT_BLUE, TFT_BLUE};
#define VIVID_ORANGE 0xFD40
uint16_t centerColors[6] = {TFT_RED, VIVID_ORANGE, VIVID_ORANGE, VIVID_ORANGE, VIVID_ORANGE, TFT_GREEN};
#define DARK_GRAY 0x2108
#define BEZEL 0x5AEB

void drawGaugeBg() {
  tft.fillScreen(DARK_GRAY);
  tft.drawCircle(120, 120, 119, BEZEL);
  tft.fillCircle(120, 120, 118, 0x3186);
  tft.fillCircle(120, 120, 113, DARK_GRAY);
  tft.drawCircle(120, 120, 113, BEZEL);
  for (int deg = 0; deg < 360; deg += 30) {
    float rad = deg * PI / 180;
    int x1 = 120 + 112 * cos(rad), y1 = 120 + 112 * sin(rad);
    int x2 = 120 + 105 * cos(rad), y2 = 120 + 105 * sin(rad);
    tft.drawLine(x1, y1, x2, y2, 0xAD55);
  }
}

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

void drawStaticFrame(int prevNum, int centerNum, int nextNum,
                     uint16_t prevColor, uint16_t centerColor, uint16_t nextColor) {
  drawGaugeBg();
  tft.fillCircle(120, 120, 56, TFT_WHITE);
  for (int r = 53; r <= 55; r++) {
    tft.drawCircle(120, 120, r, VIVID_ORANGE);
  }
  drawNumber(110, 26, prevNum, 4, prevColor);
  drawNumber(95, 85, centerNum, 10, centerColor);
  if (nextNum != -1) {
    drawNumber(110, 186, nextNum, 4, nextColor);
  }
}

void slideSet(int oldNum, int currNum, int nextNum,
              uint16_t oldColor, uint16_t currColor, uint16_t nextColor) {
  int steps = 5;
  int distance = 80;
  for (int i = 0; i <= steps; i++) {
    tft.fillCircle(120, 120, 100, DARK_GRAY);
    tft.fillRect(90, 220, 60, 20, DARK_GRAY);
    int offset = (distance * i) / steps;
    drawNumber(110, 106 - offset, oldNum, 4, oldColor);
    drawNumber(95, 85 + distance - offset, currNum, 10, currColor);
    if (nextNum != -1) {
      drawNumber(110, 106 + 2 * distance - offset, nextNum, 4, nextColor);
    }
    delay(10);
  }
}

void slideSetDown(int bottomNum, int centerNum, int topNum,
                  uint16_t bottomColor, uint16_t centerColor, uint16_t topColor) {
  int steps = 5;
  int distance = 80;
  for (int i = 0; i <= steps; i++) {
    tft.fillCircle(120, 120, 100, DARK_GRAY);
    tft.fillRect(90, 220, 60, 20, DARK_GRAY);
    tft.fillRect(90, 0, 60, 20, DARK_GRAY);
    int offset = (distance * i) / steps;
    if (topNum != -1) {
      drawNumber(110, 26 - distance + offset, topNum, 4, topColor);
    }
    drawNumber(95, 85 - distance + offset, centerNum, 10, centerColor);
    drawNumber(95, 85 + offset, bottomNum, 10, bottomColor);
    delay(10);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(DARK_GRAY);
}

void loop() {
  tft.fillScreen(DARK_GRAY);
  drawStaticFrame(-1, 0, 1, 0, centerColors[0], colors[1]);
  delay(1500);

  for (int n = 0; n <= 3; n++) {
    drawGaugeBg();
    slideSet(n, n + 1, n + 2, colors[n], centerColors[n + 1], colors[n + 2]);
    drawStaticFrame(n, n + 1, n + 2, colors[n], centerColors[n + 1], colors[n + 2]);
    delay(1500);
  }

  drawGaugeBg();
  slideSet(4, 5, -1, colors[4], centerColors[5], 0);
  drawStaticFrame(4, 5, 6, colors[4], centerColors[5], TFT_BLUE);
  delay(1500);

  for (int n = 5; n >= 2; n--) {
    drawGaugeBg();
    slideSetDown(n, n - 1, n - 2, colors[n], centerColors[n - 1], colors[n - 2]);
    drawStaticFrame(n - 2, n - 1, n, colors[n - 2], centerColors[n - 1], colors[n]);
    delay(1500);
  }

  drawGaugeBg();
  slideSetDown(1, 0, -1, colors[1], centerColors[0], 0);
  drawStaticFrame(-1, 0, 1, 0, centerColors[0], colors[1]);
  delay(1500);
}
