/*
 * V10_CLAUDE - INDICADOR MARCHAS MOTO CON TACÓMETRO
 * Mismo comportamiento que v9_claude con gráficos renovados.
 * Display redondo GC9A01 240×240 con ESP32-WROOM + LovyanGFX
 *
 * GPIOs:
 *   Display: 5(CS), 16(DC), 17(RST), 18(SCLK), 23(MOSI)
 *   Botones pullup (GND=activo): 4(→), 14(←), 19(GAS)
 *   Selectores marcha pullup: 22(1), 25(2), 26(3), 27(4), 32(5)
 *   Vuelta a N: 33
 *
 * ─── Dibujo sin parpadeo ───
 *   Toda la pantalla se describe en `sc` (Scene) y drawScene() la pinta
 *   completa. renderRows() la dibuja por franjas de 240×60 en un sprite y
 *   las envía de golpe: nunca se ve un borrado a negro.
 *   Líneas, aguja, arco de marcha y número con antialiasing.
 *
 * ─── Pantalla principal ───
 *   Bisel metálico, escala 0→12 en 240° con marcas cada 0.25, zona roja
 *   9-12, colores por zonas blanco(0), verde(1-4), naranja(5-8), rojo(9-12)
 *   Arco de marcha en 5 segmentos: rojo(N), naranja(1-4), verde(5)
 *   Marcha central con trazos vectoriales (0 = "N")
 *
 * ─── Indicadores (parpadean cada 250ms mientras se pulsan) ───
 *   →/←  flechas verdes
 *   GAS  alterna letrero "GAS" naranja / icono surtidor rojo
 *
 * ─── Arranque ───
 *   Logo Macbor 1s → aguja (barrido 0→12→0) encendiendo la escala →
 *   barrido naranja del anillo exterior. Pulsar → y ← a la vez lo repite.
 *
 * ─── Cambio de marcha ───
 *   Flanco descendente en un selector → esa marcha (o N si ya estaba).
 *   GPIO33 fuerza N. Subida: el número nuevo entra desde abajo y el viejo
 *   sale por arriba; bajada: al revés.
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
LGFX_Sprite band(&tft);
const int BAND_H = 60;

// ─── Colores (RGB565) ───
constexpr uint16_t C_BLACK    = 0x0000;
constexpr uint16_t C_WHITE    = 0xFFFF;
constexpr uint16_t C_RED      = 0xF800;
constexpr uint16_t C_GREEN    = 0x07E0;
constexpr uint16_t C_ORANGE   = 0xFD40;
constexpr uint16_t C_DIM      = 0x3186;  // marcas/números apagados
constexpr uint16_t C_TRACK    = 0x2945;  // segmentos de marcha apagados
constexpr uint16_t C_REDZONE  = 0x4000;  // fondo de la zona roja
constexpr uint16_t C_SCALE    = 0x4208;  // línea exterior de la escala
constexpr uint16_t C_SHADOW   = 0x528A;
constexpr uint16_t C_HUB      = 0x2104;
constexpr uint16_t C_RIM      = 0xC618;
// Bisel de radio 112 (interior) a 119 (exterior): brillo en el centro
const uint16_t BEZEL[8] = {0x18C3, 0x31A6, 0x52AA, 0x8C71, 0xBDF7, 0x8C71, 0x4A49, 0x2104};

#define BTN_RIGHT 4
#define BTN_LEFT 14
#define BTN_GAS 19
#define SW_N 33
const int swPins[5] = {22, 25, 26, 27, 32};

const uint16_t centerColors[6] = {C_RED, C_ORANGE, C_ORANGE, C_ORANGE, C_ORANGE, C_GREEN};

// ─── Escena: todo lo que se ve en pantalla ───

const uint8_t NO_NUM = 255;
enum GasMode : uint8_t { GAS_OFF, GAS_LABEL, GAS_PUMP };

struct Scene {
  uint8_t gear = 0;            // marcha del arco
  bool showGear = true;        // arco + número (ocultos durante el arranque)
  uint8_t numA = 0;            // número entrante/actual
  int offA = 0;                // su desplazamiento vertical
  uint8_t numB = NO_NUM;       // número saliente durante el cambio
  int offB = 0;
  float lit = 12;              // escala encendida hasta esta posición
  float needle = -1;           // aguja (<0 = oculta)
  float sweep = -1;            // barrido naranja del anillo (<0 = oculto)
  bool right = false, left = false;
  GasMode gas = GAS_OFF;
} sc;

// ─── Utilidades de dibujo ───

// Ángulo en grados (0=derecha, horario) de una posición 0..12 de la escala
float scaleDeg(float pos) { return 150 + pos * 20; }

// fillArc con ángulos que pueden pasar de 360
void fillArcDeg(LovyanGFX &g, int cx, int cy, int r0, int r1, float a0, float a1, uint16_t c) {
  a0 = fmodf(a0, 360);
  a1 = fmodf(a1, 360);
  if (a1 >= a0) {
    g.fillArc(cx, cy, r0, r1, a0, a1, c);
  } else {
    g.fillArc(cx, cy, r0, r1, a0, 360, c);
    g.fillArc(cx, cy, r0, r1, 0, a1, c);
  }
}

// Arco suavizado de grosor 2*w como cadena de líneas gruesas
void strokeArc(LovyanGFX &g, float cx, float cy, float r, float a0, float a1, float w, uint16_t c) {
  int n = max(1, (int)(fabsf(a1 - a0) / 6));
  float px = cx + r * cosf(a0 * DEG_TO_RAD), py = cy + r * sinf(a0 * DEG_TO_RAD);
  for (int i = 1; i <= n; i++) {
    float a = (a0 + (a1 - a0) * i / n) * DEG_TO_RAD;
    float x = cx + r * cosf(a), y = cy + r * sinf(a);
    g.drawWideLine(lroundf(px), lroundf(py), lroundf(x), lroundf(y), w, c);
    px = x;
    py = y;
  }
}

// ─── Marcha central: trazos sobre rejilla 9×14 (0 = "N") ───

const float GS = 5.0f;   // escala: 45×70 px
const float GW = 4.5f;   // radio del trazo

void glyphPoly(LovyanGFX &g, float ox, float oy, const float *p, int n, uint16_t c) {
  for (int i = 0; i < n - 1; i++) {
    g.drawWideLine(lroundf(ox + p[2 * i] * GS), lroundf(oy + p[2 * i + 1] * GS),
                   lroundf(ox + p[2 * i + 2] * GS), lroundf(oy + p[2 * i + 3] * GS), GW, c);
  }
}

void glyphArc(LovyanGFX &g, float ox, float oy, float gx, float gy, float r, float a0, float a1, uint16_t c) {
  strokeArc(g, ox + gx * GS, oy + gy * GS, r * GS, a0, a1, GW, c);
}

void drawGlyph(LovyanGFX &g, uint8_t d, float ox, float oy, uint16_t c) {
  switch (d) {
    case 0: { const float p[] = {0, 14, 0, 0, 9, 14, 9, 0}; glyphPoly(g, ox, oy, p, 4, c); break; }
    case 1: { const float p[] = {1.5, 3, 5, 0, 5, 14};       glyphPoly(g, ox, oy, p, 3, c); break; }
    case 2: {
      glyphArc(g, ox, oy, 4.5, 4.5, 4.5, 180, 400, c);
      const float p[] = {7.95, 7.4, 0, 14, 9, 14};
      glyphPoly(g, ox, oy, p, 3, c);
      break;
    }
    case 3:
      glyphArc(g, ox, oy, 4.5, 3.5, 3.5, 200, 450, c);
      glyphArc(g, ox, oy, 4.5, 10.5, 3.5, 270, 520, c);
      break;
    case 4: { const float p[] = {7, 14, 7, 0, 0, 10, 9, 10}; glyphPoly(g, ox, oy, p, 4, c); break; }
    case 5: {
      const float p[] = {8.5, 0, 1.5, 0, 1.72, 5.81};
      glyphPoly(g, ox, oy, p, 3, c);
      glyphArc(g, ox, oy, 4.3, 9.5, 4.5, 235, 510, c);
      break;
    }
  }
}

void drawGearNumber(LovyanGFX &g, int cx, int cy, uint8_t num, int off) {
  float ox = cx - 22.5f, oy = cy - 35 + off;
  drawGlyph(g, num, ox + 3, oy + 3, C_SHADOW);
  drawGlyph(g, num, ox, oy, centerColors[num]);
}

// ─── Esfera ───

uint16_t zoneColor(int idx) {
  if (idx == 0) return C_WHITE;
  if (idx <= 4) return C_GREEN;
  if (idx >= 9) return C_RED;
  return C_ORANGE;
}

void drawBezel(LovyanGFX &g, int cx, int cy) {
  for (int r = 112; r <= 119; r++) g.fillArc(cx, cy, r, r + 1, 0, 360, BEZEL[r - 112]);
}

void drawScale(LovyanGFX &g, int cx, int cy) {
  fillArcDeg(g, cx, cy, 96, 109, scaleDeg(9), scaleDeg(12), C_REDZONE);
  fillArcDeg(g, cx, cy, 109, 110, scaleDeg(0), scaleDeg(12), C_SCALE);

  for (int step = 0; step <= 240; step += 5) {
    float pos = step / 20.0f;
    float a = scaleDeg(pos) * DEG_TO_RAD;
    bool major = (step % 20 == 0);
    uint16_t col = C_DIM;
    if (pos <= sc.lit + 0.01f) {
      col = (step == 0) ? C_WHITE : (step < 100) ? C_GREEN : (step >= 180) ? C_RED : C_ORANGE;
    }
    float r0 = major ? 96 : 103, r1 = 109;
    g.drawWideLine(lroundf(cx + r0 * cosf(a)), lroundf(cy + r0 * sinf(a)),
                   lroundf(cx + r1 * cosf(a)), lroundf(cy + r1 * sinf(a)),
                   major ? 1.5f : 0.7f, col);
  }

  g.setFont(&fonts::FreeSansBold9pt7b);
  g.setTextDatum(MC_DATUM);
  for (int i = 0; i <= 12; i++) {
    float a = scaleDeg(i) * DEG_TO_RAD;
    g.setTextColor(i <= sc.lit + 0.01f ? zoneColor(i) : C_DIM);
    char buf[3];
    snprintf(buf, sizeof(buf), "%d", i);
    g.drawString(buf, lroundf(cx + 84 * cosf(a)), lroundf(cy + 84 * sinf(a)));
  }
}

void drawGearArc(LovyanGFX &g, int cx, int cy, uint8_t gear) {
  for (int s = 1; s <= 5; s++) {
    uint16_t c = C_TRACK;
    if (gear == 0) c = C_RED;
    else if (s <= gear) c = (gear == 5) ? C_GREEN : C_ORANGE;
    strokeArc(g, cx, cy, 62, 270 + (s - 1) * 72 + 5, 270 + s * 72 - 5, 2.2f, c);
  }
}

void drawNeedle(LovyanGFX &g, int cx, int cy, float pos) {
  float a = scaleDeg(pos) * DEG_TO_RAD;
  float c = cosf(a), s = sinf(a);
  g.drawWedgeLine(lroundf(cx - 16 * c), lroundf(cy - 16 * s),
                  lroundf(cx + 72 * c), lroundf(cy + 72 * s), 3.5f, 1.0f, C_ORANGE);
  g.fillSmoothCircle(cx, cy, 11, C_RIM);
  g.fillSmoothCircle(cx, cy, 9, C_HUB);
  g.fillSmoothCircle(cx, cy, 4, C_RED);
}

// ─── Indicadores ───

// Flecha con punta en (tipX, y); dir = +1 derecha, -1 izquierda
void drawArrow(LovyanGFX &g, int tipX, int y, int dir, uint16_t c) {
  int baseX = tipX - dir * 14;
  g.fillTriangle(tipX, y, baseX, y - 16, baseX, y + 16, c);
  g.drawWideLine(tipX, y, baseX, y - 16, 0.6f, c);
  g.drawWideLine(tipX, y, baseX, y + 16, 0.6f, c);
  g.fillSmoothRoundRect(dir > 0 ? baseX - 17 : baseX - 1, y - 8, 18, 16, 3, c);
}

void drawGasLabel(LovyanGFX &g, int cx, int cy) {
  g.fillSmoothRoundRect(cx - 22, cy + 68, 44, 24, 5, C_ORANGE);
  g.setFont(&fonts::FreeSansBold9pt7b);
  g.setTextDatum(MC_DATUM);
  g.setTextColor(C_BLACK);
  g.drawString("GAS", cx, cy + 80);
}

void drawGasPump(LovyanGFX &g, int cx, int cy) {
  int x = cx, y = cy + 82;
  g.fillSmoothRoundRect(x - 13, y - 16, 24, 31, 4, C_RED);   // cuerpo
  g.fillSmoothRoundRect(x - 9, y - 12, 16, 8, 2, C_BLACK);   // visor
  g.fillSmoothRoundRect(x - 16, y + 13, 30, 5, 2, C_RED);    // base
  g.drawWideLine(x + 11, y - 9, x + 17, y - 5, 1.2f, C_RED); // manguera
  g.drawWideLine(x + 17, y - 5, x + 17, y + 8, 1.2f, C_RED);
  g.drawWideLine(x + 17, y + 8, x + 13, y + 11, 1.2f, C_RED);
  g.drawWideLine(x - 8, y - 1, x + 6, y + 10, 1.2f, C_WHITE);
  g.drawWideLine(x + 6, y - 1, x - 8, y + 10, 1.2f, C_WHITE);
}

// ─── Render ───

void drawScene(LovyanGFX &g, int cx, int cy) {
  if (sc.showGear) {
    if (sc.numB != NO_NUM) drawGearNumber(g, cx, cy, sc.numB, sc.offB);
    drawGearNumber(g, cx, cy, sc.numA, sc.offA);
    // Durante el cambio, el número solo se ve dentro del arco de marcha
    if (sc.numB != NO_NUM || sc.offA != 0) g.fillArc(cx, cy, 57, 170, 0, 360, C_BLACK);
  }
  drawBezel(g, cx, cy);
  drawScale(g, cx, cy);
  if (sc.sweep > 0) fillArcDeg(g, cx, cy, 97, 108, scaleDeg(0), scaleDeg(sc.sweep), C_ORANGE);
  if (sc.showGear) drawGearArc(g, cx, cy, sc.gear);
  if (sc.needle >= 0) drawNeedle(g, cx, cy, sc.needle);
  if (sc.left) drawArrow(g, cx - 53, cy + 74, -1, C_GREEN);
  if (sc.right) drawArrow(g, cx + 53, cy + 74, 1, C_GREEN);
  if (sc.gas == GAS_LABEL) drawGasLabel(g, cx, cy);
  else if (sc.gas == GAS_PUMP) drawGasPump(g, cx, cy);
}

// Redibuja las filas y0..y1 de la pantalla por franjas
void renderRows(int y0, int y1) {
  for (int y = y0; y < y1; y += BAND_H) {
    int yy = min(y, 240 - BAND_H);
    band.fillScreen(C_BLACK);
    drawScene(band, 120, 120 - yy);
    band.pushSprite(0, yy);
  }
}

void renderAll() { renderRows(0, 240); }

// ─── Arranque ───

void drawMacborSplash() {
  tft.fillScreen(C_BLACK);
  int x = (240 - LOGO_MACBOR_W) / 2;
  int y = (240 - LOGO_MACBOR_H) / 2;
  tft.setSwapBytes(true);
  tft.pushImage(x, y, LOGO_MACBOR_W, LOGO_MACBOR_H, (const uint16_t*)Logomacbornegre2_map);
  tft.setSwapBytes(false);
  tft.setFont(&fonts::FreeSansBold18pt7b);
  tft.setTextColor(C_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("MACBOR", 120, y + 152);
}

// Anima `set(v)` de `from` a `to` en `ms` milisegundos, redibujando todo
template <typename F>
void animate(float from, float to, uint32_t ms, F set) {
  uint32_t t0 = millis();
  for (;;) {
    float t = min(1.0f, (millis() - t0) / (float)ms);
    set(from + (to - from) * t);
    renderAll();
    if (t >= 1) break;
  }
}

void runIntro() {
  drawMacborSplash();
  delay(1000);

  sc = Scene();
  sc.showGear = false;
  auto needle = [](float v) { sc.needle = v; sc.lit = v; };
  animate(0, 12, 1000, needle);
  animate(12, 0, 1000, needle);
  delay(500);

  sc.needle = -1;
  sc.lit = 12;
  auto sweep = [](float v) { sc.sweep = v; };
  animate(0, 12, 2400, sweep);
  delay(500);
  animate(12, 0, 2400, sweep);
  delay(500);
}

// ─── Cambio de marcha ───

// Subida: el nuevo entra desde abajo y el viejo sale por arriba
void animateGear(uint8_t from, uint8_t to) {
  const int H = 100;
  const uint32_t DUR = 220;
  int dir = (to > from) ? 1 : -1;
  sc.gear = to;
  sc.numA = to;
  sc.numB = from;
  uint32_t t0 = millis();
  for (;;) {
    float t = min(1.0f, (millis() - t0) / (float)DUR);
    float e = 1 - (1 - t) * (1 - t) * (1 - t);
    sc.offA = lroundf(dir * H * (1 - e));
    sc.offB = lroundf(-dir * H * e);
    renderRows(54, 186);
    if (t >= 1) break;
  }
  sc.numB = NO_NUM;
  sc.offA = 0;
  renderRows(54, 186);
}

// ─── Estado ───

uint8_t currentGear = 0;
bool lastSw[5];
bool lastN = HIGH, lastRight = false, lastLeft = false, lastGas = false, lastBoth = false;
uint32_t lastBlink = 0;
bool blinkOn = true;

void resetToNeutral() {
  currentGear = 0;
  for (int i = 0; i < 5; i++) lastSw[i] = digitalRead(swPins[i]);
  sc = Scene();
  renderAll();
}

void setGear(uint8_t target) {
  if (target == currentGear) return;
  animateGear(currentGear, target);
  currentGear = target;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("version programa v10_claude");
  tft.init();
  tft.setRotation(0);
  band.setColorDepth(16);
  if (!band.createSprite(240, BAND_H)) Serial.println("ERROR: sin memoria para el sprite");
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_GAS, INPUT_PULLUP);
  for (int i = 0; i < 5; i++) pinMode(swPins[i], INPUT_PULLUP);
  pinMode(SW_N, INPUT_PULLUP);
  runIntro();
  resetToNeutral();
}

void loop() {
  bool right = digitalRead(BTN_RIGHT) == LOW;
  bool left = digitalRead(BTN_LEFT) == LOW;
  bool gas = digitalRead(BTN_GAS) == LOW;

  // Combo → + ← : repetir secuencia de arranque
  bool both = right && left;
  if (both && !lastBoth) {
    lastBoth = true;
    runIntro();
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

  // Indicadores: al pulsar se encienden ya; luego parpadean cada 250ms
  if ((right && !lastRight) || (left && !lastLeft) || (gas && !lastGas)) {
    lastBlink = millis();
    blinkOn = true;
  }
  lastRight = right; lastLeft = left; lastGas = gas;
  if (millis() - lastBlink >= 250) {
    lastBlink = millis();
    blinkOn = !blinkOn;
  }

  bool r = right && blinkOn, l = left && blinkOn;
  GasMode gm = !gas ? GAS_OFF : (blinkOn ? GAS_LABEL : GAS_PUMP);
  if (r != sc.right || l != sc.left || gm != sc.gas) {
    sc.right = r;
    sc.left = l;
    sc.gas = gm;
    renderRows(166, 226);
  }
  delay(10);
}
