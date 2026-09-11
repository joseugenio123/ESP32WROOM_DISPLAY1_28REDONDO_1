/*
 * V7_DAY - INDICADOR MARCHAS MOTO CON TACÓMETRO (MODO CLARO)
 * Display redondo GC9A01 240×240 con ESP32-S3 + LovyanGFX
 *
 * GPIOs utilizados:
 *   Display: 5(CS), 16(DC), 17(RST), 18(SCLK), 23(MOSI)
 *   Botones pullup (GND=activo): 4(→), 14(←), 19(GAS)
 *   Selectores marcha pullup: 22(1), 25(2), 26(3), 27(4), 32(5)
 *   Vuelta a N: 33
 *
 * Funciones:
 *   drawNumber(x,y,num,size,color)     - dibuja un digito 0-9 con matriz 5x7
 *   drawGaugeBg()                      - esfera de tacometro con marcas y numeros
 *   drawGearArc(gear)                  - arco naranja/rojo/verde segun marcha
 *   drawRightArrow()                   - flecha verde "→" (x=196,y=111)
 *   drawLeftArrow()                    - flecha verde "←" (x=26,y=111)
 *   drawGasPump()                      - icono surtidor rojo
 *   drawGasPumpInv()                   - icono surtidor naranja invertido
 *   drawGasLabel()                     - label "GAS" naranja
 *   drawStaticFrame(prev,curr,next)    - cuadro estatico con marcha actual
 *   drawMacborSplash()                 - logo Macbor centrado 1s
 *   drawDevCredit()                    - pantalla "development by: JEU@26" 2s
 *   drawDevSplash()                    - barrido aguja tacometro (no ejecutada)
 *   setup()                            - init display, splash Macbor, pines
 *   loop()                             - lee switches y botones, sin transiciones animadas
 */
#include <LovyanGFX.hpp>
#include "logo.h"

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

uint16_t colors[6] = {TFT_RED, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
#define VIVID_ORANGE 0xFD40
uint16_t centerColors[6] = {TFT_RED, VIVID_ORANGE, VIVID_ORANGE, VIVID_ORANGE, VIVID_ORANGE, TFT_GREEN};
#define BG_COLOR 0xFFFF
#define BEZEL 0x5AEB
#define BTN_RIGHT 4
#define BTN_LEFT 14
#define BTN_GAS 19
#define SW_GEAR1 22
#define SW_GEAR2 25
#define SW_GEAR3 26
#define SW_GEAR4 27
#define SW_GEAR5 32
#define SW_N 33



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
    uint16_t col = (nums[i] == 0) ? TFT_RED : 0x0000;
    drawNumber(cx - (5 * SZ + 1) / 2, cy - (7 * SZ + 1) / 2, nums[i], SZ, col);
  }
}

void drawGaugeBg() {
  tft.fillScreen(BG_COLOR);
  // Bisel 3D - anillos concéntricos para fondo claro
  tft.drawCircle(120, 120, 119, 0x4208);           // borde exterior oscuro
  tft.fillCircle(120, 120, 118, 0x6B4D);           // gris medio
  tft.fillCircle(120, 120, 116, 0x8C71);           // gris claro
  tft.fillCircle(120, 120, 114, 0xAD55);           // gris más claro
  tft.fillCircle(120, 120, 113, BG_COLOR);         // fondo blanco
  tft.drawCircle(120, 120, 113, 0x4208);           // borde interior oscuro
  // Marcas principales cada 30° (largas) y secundarias cada 10° (cortas)
  for (int deg = 0; deg < 360; deg += 10) {
    float rad = deg * PI / 180;
    bool major = (deg % 30 == 0);
    int inner = major ? 102 : 107;
    uint16_t col = major ? 0x0000 : 0x4208;
    int x1 = 120 + 112 * cos(rad), y1 = 120 + 112 * sin(rad);
    int x2 = 120 + inner * cos(rad), y2 = 120 + inner * sin(rad);
    tft.drawLine(x1, y1, x2, y2, col);
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

void drawRightArrow() {
  tft.fillRect(196, 111, 18, 18, TFT_GREEN);
  tft.fillTriangle(222, 120, 214, 103, 214, 137, TFT_GREEN);
}

void drawLeftArrow() {
  tft.fillRect(26, 111, 18, 18, TFT_GREEN);
  tft.fillTriangle(18, 120, 26, 103, 26, 137, TFT_GREEN);
}

void drawGasPump() {
  const int cx = 182, cy = 59;
  tft.fillRect(cx - 10, cy - 9, 20, 20, TFT_RED);
  tft.drawRect(cx - 10, cy - 9, 20, 20, 0x8000);
  tft.fillRect(cx - 9, cy - 13, 18, 5, TFT_RED);
  tft.drawRect(cx - 9, cy - 13, 18, 5, 0x8000);
  tft.fillRect(cx - 5, cy - 11, 10, 2, BG_COLOR);
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
  tft.setTextDatum(TL_DATUM);
  tft.drawString("GAS", 45, 53);
}

void drawStaticFrame(int prevNum, int centerNum, int nextNum,
                     uint16_t prevColor, uint16_t centerColor, uint16_t nextColor,
                     bool redrawBg = true) {
  if (redrawBg) {
    drawGaugeBg();
  } else {
    tft.fillCircle(120, 120, 100, BG_COLOR);
  }
  tft.fillCircle(120, 120, 56, BG_COLOR);
  for (int r = 53; r <= 55; r++) {
    tft.drawCircle(120, 120, r, VIVID_ORANGE);
  }
  drawGearArc(centerNum);
  drawNumber(110, 22, prevNum, 4, prevColor);
  drawNumber(97, 87, centerNum, 10, 0xC618);
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
    tft.fillCircle(120, 120, 100, BG_COLOR);
    tft.fillRect(90, 220, 60, 20, BG_COLOR);
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
    tft.fillCircle(120, 120, 100, BG_COLOR);
    tft.fillRect(90, 220, 60, 20, BG_COLOR);
    tft.fillRect(90, 0, 60, 20, BG_COLOR);
    int offset = (distance * i) / steps;
    if (topNum != -1) {
      drawNumber(110, 22 - distance + offset, topNum, 4, topColor);
    }
    drawNumber(95, 85 - distance + offset, centerNum, 10, centerColor);
    drawNumber(95, 85 + offset, bottomNum, 10, bottomColor);
    delay(10);
  }
}

void drawMacborSplash() {
  tft.fillScreen(0x0000);
  int x = (240 - LOGO_MACBOR_W) / 2;
  int y = (240 - LOGO_MACBOR_H) / 2;
  tft.setSwapBytes(true);
  tft.pushImage(x, y, LOGO_MACBOR_W, LOGO_MACBOR_H, (const uint16_t*)Logomacbornegre2_map);
  tft.setSwapBytes(false);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.drawString("MACBOR", 120, y + 152);
}

void drawNeedleAtPos(float scalePos) {
  float step = scalePos * 20.0f;
  int clockDeg = ((int)(240 + step)) % 360;
  int codeDeg = (clockDeg + 270) % 360;
  float rad = codeDeg * PI / 180;
  int tx = 120 + 70 * cos(rad), ty = 120 + 70 * sin(rad);
  float perp = (codeDeg + 90) * PI / 180;
  int b1x = 120 + 6 * cos(perp), b1y = 120 + 6 * sin(perp);
  int b2x = 120 + 6 * cos(perp + PI), b2y = 120 + 6 * sin(perp + PI);
  tft.fillTriangle(tx, ty, b1x, b1y, b2x, b2y, VIVID_ORANGE);
  int tx2 = 120 + 68 * cos(rad), ty2 = 120 + 68 * sin(rad);
  int b1x2 = 120 + 4 * cos(perp), b1y2 = 120 + 4 * sin(perp);
  int b2x2 = 120 + 4 * cos(perp + PI), b2y2 = 120 + 4 * sin(perp + PI);
  tft.fillTriangle(tx2, ty2, b1x2, b1y2, b2x2, b2y2, 0x111111);
  tft.fillCircle(120, 120, 10, 0x111111);
  tft.drawCircle(120, 120, 10, 0x6B4D);
  tft.fillCircle(120, 120, 5, TFT_RED);
}

void drawDevSplash() {
  delay(100);
  tft.fillScreen(0x0000);
  tft.fillCircle(120, 120, 115, 0x4208);
  tft.fillCircle(120, 120, 110, 0x0000);
  tft.drawCircle(120, 120, 115, 0x6B4D);
  tft.drawCircle(120, 120, 110, 0x6B4D);
  for (int step = 0; step <= 240; step += 5) {
    int clockDeg = (240 + step) % 360;
    int codeDeg = (clockDeg + 270) % 360;
    float rad = codeDeg * PI / 180;
    bool major = (step % 20 == 0);
    int inner = major ? 98 : 98 + 8;
    uint16_t col = (step == 0) ? TFT_WHITE : (step < 100) ? TFT_GREEN : (step >= 180) ? TFT_RED : VIVID_ORANGE;
    int x1 = 120 + 108 * cos(rad), y1 = 120 + 108 * sin(rad);
    int x2 = 120 + inner * cos(rad), y2 = 120 + inner * sin(rad);
    tft.drawLine(x1, y1, x2, y2, col);
  }
  tft.fillArc(120, 120, 40, 45, 0, 360, 0x4208);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  const char* nums[13] = {"0","1","2","3","4","5","6","7","8","9","10","11","12"};
  int rn = 88;
  for (int i = 0; i <= 12; i++) {
    uint16_t tc;
    if (i == 0) tc = TFT_WHITE;
    else if (i >= 1 && i <= 4) tc = TFT_GREEN;
    else if (i >= 9) tc = TFT_RED;
    else tc = VIVID_ORANGE;
    tft.setTextColor(tc);
    int clockDeg = (240 + i * 20) % 360;
    int cd = (clockDeg + 270) % 360;
    float rad = cd * PI / 180;
    tft.drawString(nums[i], 120 + rn * cos(rad), 120 + rn * sin(rad));
  }
  uint32_t t0 = millis();
  while (millis() - t0 < 5000) {
    float pos = (millis() - t0) / 5000.0f * 12.0f;
    tft.fillCircle(120, 120, 75, 0x0000);
    tft.fillArc(120, 120, 40, 45, 0, 360, 0x4208);
    drawNeedleAtPos(pos);
    delay(20);
  }
  tft.fillCircle(120, 120, 75, 0x0000);
  tft.fillArc(120, 120, 40, 45, 0, 360, 0x4208);
  drawNeedleAtPos(12.0f);
  delay(500);
}

void drawDevCredit() {
  tft.fillScreen(0x0000);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(VIVID_ORANGE);
  tft.setTextSize(2);
  tft.drawString("development by:", 120, 80);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  tft.drawString("JEU@26", 120, 130);
  delay(2000);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  tft.init();
  tft.setRotation(0);
  drawMacborSplash();
  delay(1000);
  drawDevCredit();
  tft.fillScreen(BG_COLOR);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_GAS, INPUT_PULLUP);
  pinMode(SW_GEAR1, INPUT_PULLUP);
  pinMode(SW_GEAR2, INPUT_PULLUP);
  pinMode(SW_GEAR3, INPUT_PULLUP);
  pinMode(SW_GEAR4, INPUT_PULLUP);
  pinMode(SW_GEAR5, INPUT_PULLUP);
  pinMode(SW_N, INPUT_PULLUP);
}

void loop() {
  const int swPins[5] = {SW_GEAR1, SW_GEAR2, SW_GEAR3, SW_GEAR4, SW_GEAR5};
  bool last[5];
  for (int i = 0; i < 5; i++) last[i] = digitalRead(swPins[i]);
  int currentGear = 0;
  bool started = false;

  auto setGear = [&](int target) {
    if (target == currentGear) return;
    int old = currentGear;
    currentGear = target;
    int next = (target == 5) ? -1 : target + 1;
    int prev = (target <= 1) ? -1 : target - 1;
    uint16_t nextCol = (target == 5) ? 0 : colors[next];
    uint16_t prevCol = (target <= 1) ? 0 : colors[prev];

    if (old == 0 && target > 0) {
      slideSet(0, target, next, colors[0], centerColors[target], nextCol);
      drawStaticFrame(prev, target, next, prevCol, centerColors[target], nextCol, false);
    } else if (old > 0 && target == 0) {
      slideSetDown(old, 0, -1, colors[old], centerColors[0], 0);
      drawStaticFrame(-1, 0, 1, 0, centerColors[0], colors[1], false);
    } else if (old < target) {
      slideSet(old, target, next, colors[old], centerColors[target], nextCol);
      drawStaticFrame(prev, target, next, prevCol, centerColors[target], nextCol, false);
    } else {
      int top = (target == 0) ? -1 : target - 1;
      uint16_t topCol = (target == 0) ? 0 : colors[top];
      slideSetDown(old, target, top, colors[old], centerColors[target], topCol);
      drawStaticFrame(prev, target, next, prevCol, centerColors[target], nextCol, false);
    }
  };

  while (true) {
    bool right = digitalRead(BTN_RIGHT);
    bool left = digitalRead(BTN_LEFT);

    static uint32_t splashStart = 0;
    static bool inSplash = false;
    static int splashPhase = 0;

    if (!inSplash) {
      static bool lastBoth = false;
      bool bothNow = (right == LOW && left == LOW);
      if (bothNow && !lastBoth) {
        inSplash = true;
        splashPhase = 0;
        splashStart = millis();
        drawMacborSplash();
      }
      lastBoth = bothNow;
    }

    if (inSplash) {
      if (splashPhase == 0 && millis() - splashStart >= 1000) {
        drawDevCredit();
        inSplash = false;
        started = false;
        currentGear = 0;
        for (int i = 0; i < 5; i++) last[i] = digitalRead(swPins[i]);
        tft.fillScreen(BG_COLOR);
        drawStaticFrame(-1, 0, 1, 0, centerColors[0], colors[1]);
      }
      delay(10);
      continue;
    }

    if (!started) {
      started = true;
      drawStaticFrame(-1, 0, 1, 0, centerColors[0], colors[1]);
    }

    for (int i = 0; i < 5; i++) {
      bool c = digitalRead(swPins[i]);
      if (last[i] == HIGH && c == LOW) setGear(currentGear == i + 1 ? 0 : i + 1);
      last[i] = c;
    }

    static bool lastN = HIGH;
    bool n = digitalRead(SW_N);
    if (lastN == HIGH && n == LOW && currentGear != 0) setGear(0);
    lastN = n;

    static bool lastRight = HIGH, lastLeft = HIGH, lastGas = HIGH;
    bool gas = digitalRead(BTN_GAS);

    if (lastRight == LOW && right == HIGH) {
      tft.fillRect(196, 111, 18, 18, BG_COLOR);
      tft.fillTriangle(222, 120, 214, 103, 214, 137, BG_COLOR);
    }
    if (lastLeft == LOW && left == HIGH) {
      tft.fillRect(26, 111, 18, 18, BG_COLOR);
      tft.fillTriangle(18, 120, 26, 103, 26, 137, BG_COLOR);
    }
    if (lastGas == LOW && gas == HIGH) {
      tft.fillRect(168, 42, 32, 34, BG_COLOR);
      tft.fillRect(41, 49, 44, 24, BG_COLOR);
    }
    lastRight = right; lastLeft = left; lastGas = gas;

    static uint32_t lastBlink = 0;
    static bool blinkOn = true;
    if (millis() - lastBlink >= 250) {
      lastBlink = millis();
      blinkOn = !blinkOn;
      if (!right) {
        if (blinkOn) drawRightArrow();
        else {
          tft.fillRect(196, 111, 18, 18, BG_COLOR);
          tft.fillTriangle(222, 120, 214, 103, 214, 137, BG_COLOR);
        }
      }
      if (!left) {
        if (blinkOn) drawLeftArrow();
        else {
          tft.fillRect(26, 111, 18, 18, BG_COLOR);
          tft.fillTriangle(18, 120, 26, 103, 26, 137, BG_COLOR);
        }
      }
      if (!gas) {
        if (blinkOn) { drawGasPump(); drawGasLabel(); }
        else { tft.fillRect(168, 42, 32, 34, BG_COLOR); tft.fillRect(41, 49, 44, 24, BG_COLOR); }
      }
    }
    delay(10);
  }
}
