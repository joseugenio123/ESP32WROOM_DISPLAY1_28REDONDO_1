#include <Arduino.h>
#include <LovyanGFX.hpp>

extern "C" void *rawmemchr(const void *s, int c) {
  unsigned char *p = (unsigned char *)s;
  while (*p != (unsigned char)c) p++;
  return p;
}

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

uint16_t colors[6] = {TFT_RED, TFT_CYAN, TFT_CYAN, TFT_CYAN, TFT_CYAN, TFT_CYAN};
#define VIVID_ORANGE 0xFD40
uint16_t centerColors[6] = {TFT_RED, VIVID_ORANGE, VIVID_ORANGE, VIVID_ORANGE, VIVID_ORANGE, TFT_GREEN};
#define DARK_GRAY 0x0000
#define BEZEL 0x5AEB

void drawNumber(int x, int y, uint8_t num, uint8_t size, uint16_t color);

void drawGearLabels() {
  const float degs[6] = {0, 60, 120, 180, 240, 300};
  const uint8_t nums[6] = {2, 3, 4, 5, 0, 1};
  const int R = 110;
  const uint8_t SZ = 2;
  for (int i = 0; i < 6; i++) {
    float rad = degs[i] * PI / 180;
    int cx = 120 + round(R * cos(rad));
    int cy = 120 + round(R * sin(rad));
    uint16_t col = (nums[i] == 0) ? TFT_RED : TFT_WHITE;
    drawNumber(cx - (5 * SZ + 1) / 2, cy - (7 * SZ + 1) / 2, nums[i], SZ, col);
  }
}

void drawGaugeBg() {
  tft.fillScreen(DARK_GRAY);
  tft.drawCircle(120, 120, 119, 0x6B4D);
  tft.fillCircle(120, 120, 118, 0x4208);
  tft.fillCircle(120, 120, 113, DARK_GRAY);
  tft.drawCircle(120, 120, 113, 0x6B4D);
  for (int deg = 0; deg < 360; deg += 30) {
    float rad = deg * PI / 180;
    int x1 = 120 + 112 * cos(rad), y1 = 120 + 112 * sin(rad);
    int x2 = 120 + 105 * cos(rad), y2 = 120 + 105 * sin(rad);
    tft.drawLine(x1, y1, x2, y2, 0xAD55);
  }
  //drawGearLabels();
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

void drawGearArc(uint8_t gear) {
  if (gear == 0) {
    tft.fillArc(120, 120, 58, 66, 0, 360, TFT_RED);
    return;
  }
  for (int g = 1; g <= 5; g++) {
    float start = 270 + (g - 1) * 72.0f;
    float end = 270 + g * 72.0f;
    if (g <= gear) {
      uint16_t c = (gear == 5) ? TFT_GREEN : VIVID_ORANGE;
      tft.fillArc(120, 120, 58, 66, start, end, c);
    }
  }
}

void drawArrow() {
  tft.fillRect(196, 111, 18, 18, TFT_GREEN);
  tft.fillTriangle(222, 120, 214, 103, 214, 137, TFT_GREEN);
  tft.fillRect(26, 111, 18, 18, TFT_GREEN);
  tft.fillTriangle(18, 120, 26, 103, 26, 137, TFT_GREEN);
}

void drawGasPump() {
  const int cx = 182, cy = 59;
  tft.fillRect(cx - 10, cy - 9, 20, 20, TFT_RED);
  tft.drawRect(cx - 10, cy - 9, 20, 20, 0x8000);
  tft.fillRect(cx - 9, cy - 13, 18, 5, TFT_RED);
  tft.drawRect(cx - 9, cy - 13, 18, 5, 0x8000);
  tft.fillRect(cx - 5, cy - 11, 10, 2, DARK_GRAY);
  tft.drawLine(cx - 7, cy - 6, cx + 7, cy + 6, TFT_WHITE);
  tft.drawLine(cx + 7, cy - 6, cx - 7, cy + 6, TFT_WHITE);
  tft.fillRect(cx + 10, cy - 5, 3, 7, 0x8000);
  tft.fillRect(cx + 13, cy - 4, 1, 3, TFT_RED);
  tft.fillRect(cx - 8, cy + 11, 16, 2, 0x8000);
}

void drawGasPumpInv() {
  const int cx = 182, cy = 59;
  tft.fillRect(cx - 10, cy - 9, 20, 20, VIVID_ORANGE);
  tft.drawRect(cx - 10, cy - 9, 20, 20, 0x8200);
  tft.fillRect(cx - 9, cy - 13, 18, 5, VIVID_ORANGE);
  tft.drawRect(cx - 9, cy - 13, 18, 5, 0x8200);
  tft.fillRect(cx - 5, cy - 11, 10, 2, VIVID_ORANGE);
  tft.drawLine(cx - 7, cy - 6, cx + 7, cy + 6, TFT_RED);
  tft.drawLine(cx + 7, cy - 6, cx - 7, cy + 6, TFT_RED);
  tft.fillRect(cx + 10, cy - 5, 3, 7, 0x8200);
  tft.fillRect(cx + 13, cy - 4, 1, 3, VIVID_ORANGE);
  tft.fillRect(cx - 8, cy + 11, 16, 2, 0x8200);
}

void drawGasLabel() {
  tft.fillRect(43, 51, 40, 20, VIVID_ORANGE);
  tft.setTextColor(TFT_RED);
  tft.setTextSize(2);
  tft.drawString("GAS", 45, 53);
}

void delayBlink(int ms) {
  int half = 250;
  int cycles = ms / (half * 2);
  for (int i = 0; i < cycles; i++) {
    delay(half);
    tft.fillRect(196, 111, 18, 18, DARK_GRAY);
    tft.fillTriangle(222, 120, 214, 103, 214, 137, DARK_GRAY);
    tft.fillRect(26, 111, 18, 18, DARK_GRAY);
    tft.fillTriangle(18, 120, 26, 103, 26, 137, DARK_GRAY);
    drawGasPumpInv();
    tft.fillRect(43, 51, 40, 20, TFT_RED);
    tft.setTextColor(VIVID_ORANGE);
    tft.setTextSize(2);
    tft.drawString("GAS", 45, 53);
    delay(half);
    tft.fillRect(196, 111, 18, 18, TFT_GREEN);
    tft.fillTriangle(222, 120, 214, 103, 214, 137, TFT_GREEN);
    tft.fillRect(26, 111, 18, 18, TFT_GREEN);
    tft.fillTriangle(18, 120, 26, 103, 26, 137, TFT_GREEN);
    drawGasPump();
    drawGasLabel();
  }
}

void drawStaticFrame(int prevNum, int centerNum, int nextNum,
                     uint16_t prevColor, uint16_t centerColor, uint16_t nextColor) {
  drawGaugeBg();
  tft.fillCircle(120, 120, 56, TFT_WHITE);
  for (int r = 53; r <= 55; r++) {
    tft.drawCircle(120, 120, r, VIVID_ORANGE);
  }
  drawGearArc(centerNum);
  drawArrow();
    drawGasPump();
    drawGasLabel();
  drawNumber(110, 22, prevNum, 4, prevColor);
  drawNumber(97, 87, centerNum, 10, 0x3186);
  drawNumber(95, 85, centerNum, 10, centerColor);
  if (nextNum != -1) {
    drawNumber(110, 190, nextNum, 4, nextColor);
  }
}

void slideSet(int oldNum, int currNum, int nextNum,
              uint16_t oldColor, uint16_t currColor, uint16_t nextColor) {
  int steps = 8;
  int distance = 80;
  for (int i = 0; i <= steps; i++) {
    tft.fillCircle(120, 120, 100, DARK_GRAY);
    tft.fillRect(90, 220, 60, 20, DARK_GRAY);
    drawArrow();
    drawGasPump();
    drawGasLabel();
    int offset = (distance * i) / steps;
    drawNumber(110, 102 - offset, oldNum, 4, oldColor);
    drawNumber(95, 85 + distance - offset, currNum, 10, currColor);
    if (nextNum != -1) {
      drawNumber(110, 110 + 2 * distance - offset, nextNum, 4, nextColor);
    }
    delay(10);
  }
}

void slideSetDown(int bottomNum, int centerNum, int topNum,
                  uint16_t bottomColor, uint16_t centerColor, uint16_t topColor) {
  int steps = 8;
  int distance = 80;
  for (int i = 0; i <= steps; i++) {
    tft.fillCircle(120, 120, 100, DARK_GRAY);
    tft.fillRect(90, 220, 60, 20, DARK_GRAY);
    tft.fillRect(90, 0, 60, 20, DARK_GRAY);
    drawArrow();
    drawGasPump();
    drawGasLabel();
    int offset = (distance * i) / steps;
    if (topNum != -1) {
      drawNumber(110, 22 - distance + offset, topNum, 4, topColor);
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
  drawStaticFrame(-1, 0, 1, 0, centerColors[0], colors[1]);
  delayBlink(1500);

  for (int n = 0; n <= 3; n++) {
    slideSet(n, n + 1, n + 2, colors[n], centerColors[n + 1], colors[n + 2]);
    drawStaticFrame(n, n + 1, n + 2, colors[n], centerColors[n + 1], colors[n + 2]);
    delayBlink(1500);
  }

  slideSet(4, 5, -1, colors[4], centerColors[5], 0);
  drawStaticFrame(4, 5, -1, colors[4], centerColors[5], 0);
  delayBlink(1500);

  for (int n = 5; n >= 2; n--) {
    slideSetDown(n, n - 1, n - 2, colors[n], centerColors[n - 1], colors[n - 2]);
    drawStaticFrame(n - 2, n - 1, n, colors[n - 2], centerColors[n - 1], colors[n]);
    delayBlink(1500);
  }

  slideSetDown(1, 0, -1, colors[1], centerColors[0], 0);
  drawStaticFrame(-1, 0, 1, 0, centerColors[0], colors[1]);
  delayBlink(1500);
}
