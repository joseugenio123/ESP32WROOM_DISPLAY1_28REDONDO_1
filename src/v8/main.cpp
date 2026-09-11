/*
 * PROGRAMA V6 - CONTADOR MARCHAS MOTO
 * Display redondo GC9A01 240x240 con ESP32
 *
 * GPIOs utilizados:
 *   Display: 5(CS), 16(DC), 17(RST), 18(SCLK), 23(MOSI)
 *   Botones pullup (GND=activo): 4(→), 14(←), 19(GAS)
 *   Selectores marcha pullup: 22(1), 25(2), 26(3), 27(4), 32(5)
 *   Vuelta a N: 33
 *   Libre: 21
 *
 * Funciones:
 *   drawNumber(x,y,num,size,color)     - dibuja un digito 0-9 con matriz 5x7
 *   drawGearLabels()                   - pinta numeros 0-5 en circulo (no usado)
 *   drawGaugeBg()                      - fondo de esfera con circulos y marcas 30°
 *   drawGearArc(gear)                  - arco naranja/rojo/verde segun marcha
 *   drawRightArrow()                   - flecha verde "subir marcha" (x=196,y=111)
 *   drawLeftArrow()                    - flecha verde "bajar marcha" (x=26,y=111)
 *   drawGasPump()                      - icono surtidor rojo (x=182,y=59)
 *   drawGasPumpInv()                   - icono surtidor naranja invertido
 *   drawGasLabel()                     - label "GAS" naranja con texto rojo
 *   drawStaticFrame(prev,curr,next)    - cuadro estatico con marcha actual
 *   slideSet(old,curr,next)            - transicion ascendente de numeros
 *   slideSetDown(bottom,center,top)    - transicion descendente de numeros
 *   setup()                            - init display, pines, pullups
 *   loop()                             - lee switches y botones, anima transiciones
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

uint16_t colors[6] = {TFT_RED, TFT_CYAN, TFT_CYAN, TFT_CYAN, TFT_CYAN, TFT_CYAN};
#define VIVID_ORANGE 0xFD40
uint16_t centerColors[6] = {TFT_RED, VIVID_ORANGE, VIVID_ORANGE, VIVID_ORANGE, VIVID_ORANGE, TFT_GREEN};
#define DARK_GRAY 0x0000
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
    uint16_t col = (nums[i] == 0) ? TFT_RED : TFT_WHITE;
    drawNumber(cx - (5 * SZ + 1) / 2, cy - (7 * SZ + 1) / 2, nums[i], SZ, col);
  }
}

void drawGaugeBg() {
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
    int inner = major ? 98 : 106;
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
    tft.fillArc(120, 120, 61, 64, 0, 360, TFT_RED);
    return;
  }
  for (int g = 1; g <= 5; g++) {
    float start = 270 + (g - 1) * 72.0f;
    float end = 270 + g * 72.0f;
    if (g <= gear) {
      uint16_t c = (gear == 5) ? TFT_GREEN : VIVID_ORANGE;
      tft.fillArc(120, 120, 61, 64, start, end, c);
    }
  }
}

void drawRightArrow() {
  tft.fillRect(147, 185, 18, 18, TFT_GREEN);
  tft.fillTriangle(173, 194, 165, 177, 165, 211, TFT_GREEN);
}

void drawLeftArrow() {
  tft.fillRect(75, 185, 18, 18, TFT_GREEN);
  tft.fillTriangle(67, 194, 75, 177, 75, 211, TFT_GREEN);
}

void drawGasPump() {
  const int cx = 120, cy = 198;
  tft.fillRect(cx - 14, cy - 12, 28, 28, TFT_RED);
  tft.drawRect(cx - 14, cy - 12, 28, 28, 0x8000);
  tft.fillRect(cx - 12, cy - 17, 24, 7, TFT_RED);
  tft.drawRect(cx - 12, cy - 17, 24, 7, 0x8000);
  tft.fillRect(cx - 7, cy - 14, 14, 3, DARK_GRAY);
  tft.drawLine(cx - 10, cy - 8, cx + 10, cy + 8, TFT_WHITE);
  tft.drawLine(cx + 10, cy - 8, cx - 10, cy + 8, TFT_WHITE);
  tft.fillRect(cx + 14, cy - 6, 4, 9, 0x8000);
  tft.fillRect(cx + 18, cy - 5, 2, 4, TFT_RED);
  tft.fillRect(cx - 10, cy + 16, 22, 3, 0x8000);
}

void drawGasPumpInv() {
  const int cx = 120, cy = 198;
  tft.fillRect(cx - 14, cy - 12, 28, 28, VIVID_ORANGE);
  tft.drawRect(cx - 14, cy - 12, 28, 28, 0x8200);
  tft.fillRect(cx - 12, cy - 17, 24, 7, VIVID_ORANGE);
  tft.drawRect(cx - 12, cy - 17, 24, 7, 0x8200);
  tft.fillRect(cx - 7, cy - 14, 14, 3, VIVID_ORANGE);
  tft.drawLine(cx - 10, cy - 8, cx + 10, cy + 8, TFT_RED);
  tft.drawLine(cx + 10, cy - 8, cx - 10, cy + 8, TFT_RED);
  tft.fillRect(cx + 14, cy - 6, 4, 9, 0x8200);
  tft.fillRect(cx + 18, cy - 5, 2, 4, VIVID_ORANGE);
  tft.fillRect(cx - 10, cy + 16, 22, 3, 0x8200);
}

void drawGasLabel() {
  tft.fillRect(100, 188, 40, 20, VIVID_ORANGE);
  tft.setTextColor(TFT_RED);
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("GAS", 102, 190);
}

void drawStaticFrame(int prevNum, int centerNum, int nextNum,
                     uint16_t prevColor, uint16_t centerColor, uint16_t nextColor) {
  drawGaugeBg();
  drawGearArc(centerNum);
  drawNumber(97, 87, centerNum, 10, 0x3186);
  drawNumber(95, 85, centerNum, 10, centerColor);
}

void drawTachoNumber(int idx) {
  const char* nums[13] = {"0","1","2","3","4","5","6","7","8","9","10","11","12"};
  uint16_t tc;
  if (idx == 0) tc = TFT_WHITE;
  else if (idx >= 1 && idx <= 4) tc = TFT_GREEN;
  else if (idx >= 9) tc = TFT_RED;
  else tc = VIVID_ORANGE;
  tft.setTextColor(tc);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  int clockDeg = (240 + idx * 20) % 360;
  int cd = (clockDeg + 270) % 360;
  float rad = cd * PI / 180;
  tft.drawString(nums[idx], 120 + 88 * cos(rad), 120 + 88 * sin(rad));
}

void slideSet(int oldNum, int currNum, int nextNum,
              uint16_t oldColor, uint16_t currColor, uint16_t nextColor) {
  int steps = 8;
  int distance = 80;
  int prevY = -1;
  for (int i = 0; i <= steps; i++) {
    int offset = (distance * i) / steps;
    int y = 85 + distance - offset;
    if (prevY != -1) {
      tft.fillRect(92, prevY - 2, 56, 74, 0x0000);
      tft.fillArc(120, 120, 40, 45, 0, 360, 0x4208);
      drawTachoNumber(4);
      drawTachoNumber(5);
      drawTachoNumber(6);
    }
    drawNumber(95, y, currNum, 10, currColor);
    prevY = y;
    delay(10);
  }
}

void slideSetDown(int bottomNum, int centerNum, int topNum,
                  uint16_t bottomColor, uint16_t centerColor, uint16_t topColor) {
  int steps = 8;
  int distance = 60;
  int prevY = -1;
  for (int i = 0; i <= steps; i++) {
    int offset = (distance * i) / steps;
    int y = 85 - distance + offset;
    if (prevY != -1) {
      tft.fillRect(92, prevY - 2, 56, 74, 0x0000);
      tft.fillArc(120, 120, 40, 45, 0, 360, 0x4208);
      drawTachoNumber(4);
      drawTachoNumber(5);
      drawTachoNumber(6);
    }
    drawNumber(95, y, centerNum, 10, centerColor);
    prevY = y;
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

void setup() {
  Serial.begin(115200);
  delay(1000);
  tft.init();
  tft.setRotation(0);
  drawMacborSplash();
  delay(1000);
  drawDevSplash();
  tft.fillScreen(DARK_GRAY);
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
      drawStaticFrame(prev, target, next, prevCol, centerColors[target], nextCol);
    } else if (old > 0 && target == 0) {
      slideSetDown(old, 0, -1, colors[old], centerColors[0], 0);
      drawStaticFrame(-1, 0, 1, 0, centerColors[0], colors[1]);
    } else if (old < target) {
      slideSet(old, target, next, colors[old], centerColors[target], nextCol);
      drawStaticFrame(prev, target, next, prevCol, centerColors[target], nextCol);
    } else {
      int top = (target == 0) ? -1 : target - 1;
      uint16_t topCol = (target == 0) ? 0 : colors[top];
      slideSetDown(old, target, top, colors[old], centerColors[target], topCol);
      drawStaticFrame(prev, target, next, prevCol, centerColors[target], nextCol);
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
        drawDevSplash();
        inSplash = false;
        started = false;
        currentGear = 0;
        for (int i = 0; i < 5; i++) last[i] = digitalRead(swPins[i]);
        tft.fillScreen(DARK_GRAY);
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

    auto restoreFrame = [&]() {
      int nxt = (currentGear == 5) ? -1 : currentGear + 1;
      int prv = (currentGear <= 1) ? -1 : currentGear - 1;
      uint16_t nxtCol = (currentGear == 5) ? 0 : colors[nxt];
      uint16_t prvCol = (currentGear <= 1) ? 0 : colors[prv];
      drawStaticFrame(prv, currentGear, nxt, prvCol, centerColors[currentGear], nxtCol);
    };

    if (lastRight == LOW && right == HIGH) restoreFrame();
    if (lastRight == HIGH && right == LOW) drawRightArrow();
    if (lastLeft == LOW && left == HIGH) restoreFrame();
    if (lastLeft == HIGH && left == LOW) drawLeftArrow();
    if (lastGas == LOW && gas == HIGH) restoreFrame();
    if (lastGas == HIGH && gas == LOW) { drawGasLabel(); }
    lastRight = right; lastLeft = left; lastGas = gas;

    static uint32_t lastBlink = 0;
    static bool blinkOn = true;
    if (millis() - lastBlink >= 250) {
      lastBlink = millis();
      blinkOn = !blinkOn;
      if (!right) {
        if (blinkOn) drawRightArrow();
        else {
          tft.fillRect(147, 185, 18, 18, 0x0000);
          tft.fillTriangle(173, 194, 165, 177, 165, 211, 0x0000);
        }
      }
      if (!left) {
        if (blinkOn) drawLeftArrow();
        else {
          tft.fillRect(75, 185, 18, 18, 0x0000);
          tft.fillTriangle(67, 194, 75, 177, 75, 211, 0x0000);
        }
      }
      if (!gas) {
        if (blinkOn) {
          tft.fillRect(102, 177, 40, 42, 0x0000);
          drawGasLabel();
        } else {
          tft.fillRect(98, 186, 44, 24, 0x0000);
          drawGasPump();
        }
      }
    }
    delay(10);
  }
}
