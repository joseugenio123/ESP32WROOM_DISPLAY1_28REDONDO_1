/*
 * V9_CLAUDE - INDICADOR MARCHAS MOTO CON TACÓMETRO
 * Versión limpia de v9 (mismo comportamiento, sin código muerto).
 * Display redondo GC9A01 240×240 con ESP32-WROOM + LovyanGFX
 *
 * GPIOs:
 *   Display: 5(CS), 16(DC), 17(RST), 18(SCLK), 23(MOSI)
 *   Botones pullup (GND=activo): 4(→), 14(←), 19(GAS)
 *   Selectores marcha pullup: 22(1), 25(2), 26(3), 27(4), 32(5)
 *   Vuelta a N: 33
 *
 * ─── Pantalla principal ───
 *   drawGaugeBg()        - esfera de tacómetro: marcas cada 5° sobre arco
 *                          240° (0→12), colores por zonas blanco(0),
 *                          verde(1-4), naranja(5-8), rojo(9-12)
 *   drawGearArc(gear)    - arco fino (radio 61-64) alrededor de la marcha:
 *                          rojo(N), naranja(1-4), verde(5)
 *   drawNumber(…)        - digito con matriz 5×7 escalable (el 0 se ve "N")
 *   drawStaticFrame(g)   - esfera + arco + marcha centrada
 *
 * ─── Indicadores (parpadean cada 250ms mientras se pulsan) ───
 *   →/←  flechas verdes
 *   GAS  alterna letrero "GAS" naranja / icono surtidor rojo
 *   Al soltar el boton se redibuja la pantalla completa.
 *
 * ─── Arranque ───
 *   Logo Macbor 1s → esfera con aguja (barrido 0→12→0) → barrido del
 *   anillo exterior. Pulsar → y ← a la vez repite la secuencia.
 *
 * ─── Cambio de marcha ───
 *   Flanco descendente en un selector → esa marcha (o N si ya estaba).
 *   GPIO33 fuerza N. Subida: el numero entra desde abajo; bajada: desde
 *   arriba.
 */
#include <LovyanGFX.hpp>
#include "logo.h"

// Parche de enlazado: la toolchain no aporta rawmemchr.
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

#define VIVID_ORANGE 0xFD40
#define BLACK 0x0000
#define RING_GRAY 0x4208
#define RIM_GRAY 0x6B4D
#define NEEDLE_DARK 0x111111
#define SHADOW 0x3186

#define BTN_RIGHT 4
#define BTN_LEFT 14
#define BTN_GAS 19
#define SW_N 33
const int swPins[5] = {22, 25, 26, 27, 32};

const uint16_t centerColors[6] = {TFT_RED, VIVID_ORANGE, VIVID_ORANGE, VIVID_ORANGE, VIVID_ORANGE, TFT_GREEN};

// Fuente 5×7: el 0 se dibuja como "N" (punto muerto)
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

// ─── Geometría del tacómetro ───

// Ángulo (radianes, 0=derecha, horario) de una posición 0..12 de la escala
float scaleRad(float pos) {
  int codeDeg = ((int)(240 + pos * 20) + 270) % 360;
  return codeDeg * PI / 180;
}

uint16_t zoneColor(int idx) {
  if (idx == 0) return TFT_WHITE;
  if (idx <= 4) return TFT_GREEN;
  if (idx >= 9) return TFT_RED;
  return VIVID_ORANGE;
}

void drawTachoNumber(int idx) {
  char buf[3];
  snprintf(buf, sizeof(buf), "%d", idx);
  tft.setTextColor(zoneColor(idx));
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  float rad = scaleRad(idx);
  tft.drawString(buf, 120 + 88 * cos(rad), 120 + 88 * sin(rad));
}

void drawTachoNumbers() {
  for (int i = 0; i <= 12; i++) drawTachoNumber(i);
}

void drawInnerRing() {
  tft.fillArc(120, 120, 40, 45, 0, 360, RING_GRAY);
}

void drawGaugeBg() {
  tft.fillScreen(BLACK);
  tft.fillCircle(120, 120, 115, RING_GRAY);
  tft.fillCircle(120, 120, 110, BLACK);
  tft.drawCircle(120, 120, 115, RIM_GRAY);
  tft.drawCircle(120, 120, 110, RIM_GRAY);
  for (int step = 0; step <= 240; step += 5) {
    float rad = scaleRad(step / 20.0f);
    int inner = (step % 20 == 0) ? 98 : 106;
    uint16_t col = (step == 0) ? TFT_WHITE : (step < 100) ? TFT_GREEN : (step >= 180) ? TFT_RED : VIVID_ORANGE;
    tft.drawLine(120 + 108 * cos(rad), 120 + 108 * sin(rad),
                 120 + inner * cos(rad), 120 + inner * sin(rad), col);
  }
  drawInnerRing();
  drawTachoNumbers();
}

// ─── Marcha central ───

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
  uint16_t c = (gear == 5) ? TFT_GREEN : VIVID_ORANGE;
  for (int g = 1; g <= gear; g++) {
    tft.fillArc(120, 120, 61, 64, 270 + (g - 1) * 72.0f, 270 + g * 72.0f, c);
  }
}

void drawStaticFrame(uint8_t gear) {
  drawGaugeBg();
  drawGearArc(gear);
  drawNumber(97, 87, gear, 10, SHADOW);
  drawNumber(95, 85, gear, 10, centerColors[gear]);
}

// Anima el numero desde y=fromY hasta y=85, borrando solo el area del
// frame anterior y restaurando lo que tapa (anillo interior y numeros 4/5/6)
void slideNumber(uint8_t gear, int fromY) {
  const int steps = 8;
  int prevY = -1;
  for (int i = 0; i <= steps; i++) {
    int y = fromY + ((85 - fromY) * i) / steps;
    if (prevY != -1) {
      tft.fillRect(92, prevY - 2, 56, 74, BLACK);
      drawInnerRing();
      drawTachoNumber(4);
      drawTachoNumber(5);
      drawTachoNumber(6);
    }
    drawNumber(95, y, gear, 10, centerColors[gear]);
    prevY = y;
    delay(10);
  }
}

void slideUp(uint8_t gear)   { slideNumber(gear, 165); }  // entra desde abajo
void slideDown(uint8_t gear) { slideNumber(gear, 25); }   // entra desde arriba (no tapa el "6")

// ─── Indicadores ───

void drawRightArrow(uint16_t c = TFT_GREEN) {
  tft.fillRect(147, 185, 18, 18, c);
  tft.fillTriangle(173, 194, 165, 177, 165, 211, c);
}

void drawLeftArrow(uint16_t c = TFT_GREEN) {
  tft.fillRect(75, 185, 18, 18, c);
  tft.fillTriangle(67, 194, 75, 177, 75, 211, c);
}

void drawGasPump() {
  const int cx = 120, cy = 198;
  tft.fillRect(cx - 14, cy - 12, 28, 28, TFT_RED);
  tft.drawRect(cx - 14, cy - 12, 28, 28, 0x8000);
  tft.fillRect(cx - 12, cy - 17, 24, 7, TFT_RED);
  tft.drawRect(cx - 12, cy - 17, 24, 7, 0x8000);
  tft.fillRect(cx - 7, cy - 14, 14, 3, BLACK);
  tft.drawLine(cx - 10, cy - 8, cx + 10, cy + 8, TFT_WHITE);
  tft.drawLine(cx + 10, cy - 8, cx - 10, cy + 8, TFT_WHITE);
  tft.fillRect(cx + 14, cy - 6, 4, 9, 0x8000);
  tft.fillRect(cx + 18, cy - 5, 2, 4, TFT_RED);
  tft.fillRect(cx - 10, cy + 16, 22, 3, 0x8000);
}

void drawGasLabel() {
  tft.fillRect(100, 188, 40, 20, VIVID_ORANGE);
  tft.setTextColor(TFT_RED);
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("GAS", 102, 190);
}

// ─── Arranque ───

void drawMacborSplash() {
  tft.fillScreen(BLACK);
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

// Borra el centro y dibuja la aguja (triangulo naranja + cubo central)
void drawNeedleFrame(float pos) {
  tft.fillCircle(120, 120, 75, BLACK);
  drawInnerRing();
  float rad = scaleRad(pos);
  float perp = rad + PI / 2;
  int tx = 120 + 70 * cos(rad), ty = 120 + 70 * sin(rad);
  tft.fillTriangle(tx, ty, 120 + 6 * cos(perp), 120 + 6 * sin(perp),
                   120 - 6 * cos(perp), 120 - 6 * sin(perp), VIVID_ORANGE);
  int tx2 = 120 + 68 * cos(rad), ty2 = 120 + 68 * sin(rad);
  tft.fillTriangle(tx2, ty2, 120 + 4 * cos(perp), 120 + 4 * sin(perp),
                   120 - 4 * cos(perp), 120 - 4 * sin(perp), NEEDLE_DARK);
  tft.fillCircle(120, 120, 10, NEEDLE_DARK);
  tft.drawCircle(120, 120, 10, RIM_GRAY);
  tft.fillCircle(120, 120, 5, TFT_RED);
}

void drawDevSplash() {
  delay(100);
  drawGaugeBg();
  uint32_t t0 = millis();
  while (millis() - t0 < 1000) {
    drawNeedleFrame((millis() - t0) / 1000.0f * 12.0f);
    delay(20);
  }
  drawNeedleFrame(12.0f);
  t0 = millis();
  while (millis() - t0 < 1000) {
    drawNeedleFrame(12.0f - (millis() - t0) / 1000.0f * 12.0f);
    delay(20);
  }
  drawNeedleFrame(0.0f);
}

void sweepOuterRing() {
  const int rmin = 98, rmax = 108;
  auto frame = [&](float v) {
    tft.fillCircle(120, 120, rmax, BLACK);
    if (v > 0) {
      const int startM = 150;  // posicion 0 de la escala
      int endM = (int)(240 + v * 20 + 270) % 360;
      if (endM > startM) {
        tft.fillArc(120, 120, rmin, rmax, startM, endM, VIVID_ORANGE);
      } else {
        tft.fillArc(120, 120, rmin, rmax, startM, 360, VIVID_ORANGE);
        tft.fillArc(120, 120, rmin, rmax, 0, endM, VIVID_ORANGE);
      }
    }
    drawTachoNumbers();
  };
  for (int i = 1; i <= 48; i++) {
    frame(i / 48.0f * 12.0f);
    delay(50);
  }
  delay(500);
  for (int i = 47; i >= 0; i--) {
    frame(i / 48.0f * 12.0f);
    delay(50);
  }
  frame(0);
}

// ─── Estado ───

uint8_t currentGear = 0;
bool lastSw[5];
bool lastN = HIGH, lastRight = HIGH, lastLeft = HIGH, lastGas = HIGH, lastBoth = false;
uint32_t lastBlink = 0;
bool blinkOn = true;

void resetToNeutral() {
  currentGear = 0;
  for (int i = 0; i < 5; i++) lastSw[i] = digitalRead(swPins[i]);
  drawStaticFrame(0);
}

void setGear(uint8_t target) {
  if (target == currentGear) return;
  if (target > currentGear) slideUp(target);
  else slideDown(target);
  currentGear = target;
  drawStaticFrame(target);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  tft.init();
  tft.setRotation(0);
  drawMacborSplash();
  delay(1000);
  drawDevSplash();
  delay(500);
  sweepOuterRing();
  delay(500);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_GAS, INPUT_PULLUP);
  for (int i = 0; i < 5; i++) pinMode(swPins[i], INPUT_PULLUP);
  pinMode(SW_N, INPUT_PULLUP);
  resetToNeutral();
}

void loop() {
  bool right = digitalRead(BTN_RIGHT);
  bool left = digitalRead(BTN_LEFT);
  bool gas = digitalRead(BTN_GAS);

  // Combo → + ← : repetir secuencia de arranque
  bool both = (right == LOW && left == LOW);
  if (both && !lastBoth) {
    lastBoth = true;
    drawMacborSplash();
    delay(1000);
    drawDevSplash();
    sweepOuterRing();
    resetToNeutral();
    return;
  }
  lastBoth = both;

  for (int i = 0; i < 5; i++) {
    bool c = digitalRead(swPins[i]);
    if (lastSw[i] == HIGH && c == LOW) setGear(currentGear == i + 1 ? 0 : i + 1);
    lastSw[i] = c;
  }

  bool n = digitalRead(SW_N);
  if (lastN == HIGH && n == LOW) setGear(0);
  lastN = n;

  if (lastRight == LOW && right == HIGH) drawStaticFrame(currentGear);
  if (lastRight == HIGH && right == LOW) drawRightArrow();
  if (lastLeft == LOW && left == HIGH) drawStaticFrame(currentGear);
  if (lastLeft == HIGH && left == LOW) drawLeftArrow();
  if (lastGas == LOW && gas == HIGH) drawStaticFrame(currentGear);
  if (lastGas == HIGH && gas == LOW) drawGasLabel();
  lastRight = right; lastLeft = left; lastGas = gas;

  if (millis() - lastBlink >= 250) {
    lastBlink = millis();
    blinkOn = !blinkOn;
    if (!right) drawRightArrow(blinkOn ? TFT_GREEN : BLACK);
    if (!left) drawLeftArrow(blinkOn ? TFT_GREEN : BLACK);
    if (!gas) {
      if (blinkOn) {
        tft.fillRect(102, 177, 40, 42, BLACK);
        drawGasLabel();
      } else {
        tft.fillRect(98, 186, 44, 24, BLACK);
        drawGasPump();
      }
    }
  }
  delay(10);
}
