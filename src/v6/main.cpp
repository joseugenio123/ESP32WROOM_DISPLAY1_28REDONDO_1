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

void drawStaticFrame(int prevNum, int centerNum, int nextNum,
                     uint16_t prevColor, uint16_t centerColor, uint16_t nextColor) {
  drawGaugeBg();
  tft.fillCircle(120, 120, 56, TFT_WHITE);
  for (int r = 53; r <= 55; r++) {
    tft.drawCircle(120, 120, r, VIVID_ORANGE);
  }
  drawGearArc(centerNum);
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
  bool last[5] = {HIGH, HIGH, HIGH, HIGH, HIGH};
  int currentGear = 0;
  const int swPins[5] = {SW_GEAR1, SW_GEAR2, SW_GEAR3, SW_GEAR4, SW_GEAR5};
  drawStaticFrame(-1, 0, 1, 0, centerColors[0], colors[1]);

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
    bool right = digitalRead(BTN_RIGHT);
    bool left = digitalRead(BTN_LEFT);
    bool gas = digitalRead(BTN_GAS);

    if (lastRight == LOW && right == HIGH) {
      tft.fillRect(196, 111, 18, 18, DARK_GRAY);
      tft.fillTriangle(222, 120, 214, 103, 214, 137, DARK_GRAY);
    }
    if (lastLeft == LOW && left == HIGH) {
      tft.fillRect(26, 111, 18, 18, DARK_GRAY);
      tft.fillTriangle(18, 120, 26, 103, 26, 137, DARK_GRAY);
    }
    if (lastGas == LOW && gas == HIGH) {
      tft.fillRect(168, 42, 32, 34, DARK_GRAY);
      tft.fillRect(41, 49, 44, 24, DARK_GRAY);
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
          tft.fillRect(196, 111, 18, 18, DARK_GRAY);
          tft.fillTriangle(222, 120, 214, 103, 214, 137, DARK_GRAY);
        }
      }
      if (!left) {
        if (blinkOn) drawLeftArrow();
        else {
          tft.fillRect(26, 111, 18, 18, DARK_GRAY);
          tft.fillTriangle(18, 120, 26, 103, 26, 137, DARK_GRAY);
        }
      }
      if (!gas) {
        if (blinkOn) { drawGasPump(); drawGasLabel(); }
        else { tft.fillRect(168, 42, 32, 34, DARK_GRAY); tft.fillRect(41, 49, 44, 24, DARK_GRAY); }
      }
    }
    delay(10);
  }
}
