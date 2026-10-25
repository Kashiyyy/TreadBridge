/*
  Display-Oberflaeche fuer das Waveshare ESP32-C6-Touch-LCD-1.47 (172 x 320, Hoch- oder Querformat)
  Beim Start: Profilauswahl. Danach vier Seiten, umgeschaltet durch Wischen nach links/rechts
  (die Leiste unten zeigt, wo man ist):
    LIVE    Anzeigen fuer Laufband/Uhr, Tempo, Pace, Distanz, Steigung, Zeit
    GERAET  Bluetooth-Geraet suchen, auswaehlen, trennen (Liste per Wischen nach oben/unten scrollen)
    FOOT    Foot Pod an/aus, welche Werte an die Uhr gehen, Kadenz, Testtempo
    SYS     Helligkeit, Auto-Verbinden, Webserver, Ansicht (waagrecht), Geraet vergessen, Profil wechseln
  Texte duerfen UTF-8 mit Umlauten enthalten, sie werden fuer die Display-Schrift (CP437) umgewandelt.
  Gezeichnet wird mit Arduino_GFX direkt (ohne LVGL). Es werden nur geaenderte Elemente neu gezeichnet.
*/
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <SPI.h>
#include "pins.h"
#include "config.h"
#include "defaults.h"
#include "app.h"
#include "touch.h"

namespace {

Arduino_DataBus* bus = nullptr;
Arduino_GFX* gfx = nullptr;
Preferences uiPrefs;
UiState S;

bool landscape = false;
int  W = LCD_W, H = LCD_H;               // aktuelle Breite/Hoehe (abhaengig von der Ausrichtung)
int  TAB_Y = 286, TAB_H = 34;            // Leiste unten
const int SWIPE_MIN = 40;                // Pixel fuer eine Wischgeste

int  page = 0;                           // 0 LIVE, 1 GERAET, 2 FOOT, 3 SYS
bool needFull = true;
int  devScroll = 0;
int  brightness = 50;                    // Prozent
bool forgetArmed = false, profileArmed = false;
uint32_t armedAt = 0;

uint16_t C_BG, C_FG, C_DIM, C_ACC, C_OK, C_WARN, C_BAD, C_CARD, C_CARD_HI, C_OFF, C_TAB, C_LINE;
uint32_t keys[128];

// ---------- Text: UTF-8 -> CP437 (Zeichensatz der Display-Schrift) ----------
void toCp437(const char* in, char* out, size_t n) {
  size_t o = 0, i = 0;
  while (in[i] && o + 1 < n) {
    uint8_t c = (uint8_t)in[i];
    if (c < 0x80) { out[o++] = (char)c; i++; continue; }
    char m = '?';
    int len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : 2;
    if (c == 0xC3 && in[i + 1]) {
      switch ((uint8_t)in[i + 1]) {
        case 0xA4: m = (char)0x84; break;   // ae
        case 0xB6: m = (char)0x94; break;   // oe
        case 0xBC: m = (char)0x81; break;   // ue
        case 0x84: m = (char)0x8E; break;   // AE
        case 0x96: m = (char)0x99; break;   // OE
        case 0x9C: m = (char)0x9A; break;   // UE
        case 0x9F: m = (char)0xE1; break;   // ss
        case 0xA9: m = (char)0x82; break;   // e akut
        case 0xA8: m = (char)0x8A; break;
        case 0xA0: m = (char)0x85; break;
        case 0xA2: m = (char)0x83; break;
        case 0xAA: m = (char)0x88; break;
        case 0xA7: m = (char)0x87; break;
        case 0xB1: m = (char)0xA4; break;
      }
    } else if (c == 0xC2 && (uint8_t)in[i + 1] == 0xB0) {
      m = (char)0xF8;                       // Grad-Zeichen
    }
    out[o++] = m;
    for (int k = 0; k < len && in[i]; k++) i++;
  }
  out[o] = 0;
}

// ---------- Zeichenhelfer ----------
bool changed(int id, uint32_t key) {
  if (needFull || keys[id] != key) { keys[id] = key; return true; }
  return false;
}

uint32_t fnv(const char* s, uint32_t h = 2166136261u) {
  while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
  return h;
}

void rawText(int x, int y, uint8_t sz, uint16_t fg, uint16_t bg, const char* s) {
  char buf[96];
  toCp437(s, buf, sizeof buf);
  gfx->setTextSize(sz);
  gfx->setTextColor(fg, bg);
  gfx->setCursor(x, y);
  gfx->print(buf);
}

// Text mit Zeichenlimit (zu lange Namen werden abgeschnitten, damit nichts ueberlappt)
void rawTextN(int x, int y, uint8_t sz, uint16_t fg, uint16_t bg, const char* s, int maxChars) {
  char cs[96];
  toCp437(s, cs, sizeof cs);
  if ((int)strlen(cs) > maxChars) cs[maxChars] = 0;
  gfx->setTextSize(sz);
  gfx->setTextColor(fg, bg);
  gfx->setCursor(x, y);
  gfx->print(cs);
}

// Text in festem Feld (chars Zeichen), align: 0 links, 1 mittig, 2 rechts. Zeichnet nur bei Aenderung.
void put(int id, int x, int y, uint8_t sz, uint16_t fg, uint16_t bg, const char* s, int chars, int align = 0) {
  char cs[96], buf[96];
  toCp437(s, cs, sizeof cs);
  if (chars > 90) chars = 90;
  int len = (int)strlen(cs);
  if (len > chars) len = chars;
  int left = align == 0 ? 0 : (align == 1 ? (chars - len) / 2 : chars - len);
  memset(buf, ' ', chars);
  memcpy(buf + left, cs, len);
  buf[chars] = 0;
  uint32_t k = fnv(buf) ^ ((uint32_t)fg << 16) ^ bg ^ ((uint32_t)sz << 28);
  if (!changed(id, k)) return;
  gfx->setTextSize(sz);
  gfx->setTextColor(fg, bg);
  gfx->setCursor(x, y);
  gfx->print(buf);
}

void button(int id, int x, int y, int w, int h, const char* label, uint16_t fill, uint16_t fg, uint8_t sz = 1) {
  uint32_t k = fnv(label) ^ ((uint32_t)fill << 16) ^ fg ^ ((uint32_t)w << 24) ^ ((uint32_t)sz << 28);
  if (!changed(id, k)) return;
  char cs[96];
  toCp437(label, cs, sizeof cs);
  gfx->fillRoundRect(x, y, w, h, 6, fill);
  int tw = (int)strlen(cs) * 6 * sz, th = 8 * sz;
  rawText(x + (w - tw) / 2, y + (h - th) / 2, sz, fg, fill, label);
}

void pill(int id, int x, int y, bool on, uint16_t bg) {
  if (!changed(id, on ? 1 : 2)) return;
  gfx->fillRoundRect(x, y, 40, 18, 9, bg);              // Untergrund loeschen
  gfx->fillRoundRect(x, y, 40, 18, 9, on ? C_OK : C_OFF);
  gfx->fillCircle(on ? x + 31 : x + 9, y + 9, 7, C_FG);
}

void card(int x, int y, int w, int h, uint16_t col) { gfx->fillRoundRect(x, y, w, h, 6, col); }

bool inRect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

void applyBrightness() { ledcWrite(PIN_LCD_BL, (uint32_t)brightness * 255 / 100); }

void setOrientation(bool land) {
  landscape = land;
  gfx->setRotation(land ? LANDSCAPE_ROT : 0);
  W = land ? LCD_H : LCD_W;
  H = land ? LCD_W : LCD_H;
  TAB_H = land ? 20 : 34;
  TAB_Y = H - TAB_H;
  devScroll = 0;
  needFull = true;
}

// Touch liefert Hochformat-Koordinaten; fuer die Querformat-Ansicht umrechnen.
void mapTouch(int& x, int& y) {
  if (!landscape) return;
  int px = x, py = y;
  if (LANDSCAPE_ROT == 1) { x = py; y = LCD_W - 1 - px; }
  else                    { x = LCD_H - 1 - py; y = px; }
}

bool readTouch(int& x, int& y) {
  bool d = touchPoll(x, y);
  if (d) mapTouch(x, y);
  return d;
}

// ---------- Display-Initialisierung (JD9853, ST7789-Befehlssatz) ----------
// Registerfolge laut Waveshare-Beispiel fuer dieses Panel.
void lcdRegInit() {
  static const uint8_t init_operations[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x11,
    END_WRITE,
    DELAY, 120,
    BEGIN_WRITE,
    WRITE_C8_D16, 0xDF, 0x98, 0x53,
    WRITE_C8_D8,  0xB2, 0x23,
    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 4, 0x00, 0x47, 0x00, 0x6F,
    WRITE_COMMAND_8, 0xBB,
    WRITE_BYTES, 6, 0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,
    WRITE_C8_D16, 0xC0, 0x44, 0xA4,
    WRITE_C8_D8,  0xC1, 0x16,
    WRITE_COMMAND_8, 0xC3,
    WRITE_BYTES, 8, 0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,
    WRITE_COMMAND_8, 0xC4,
    WRITE_BYTES, 12, 0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A,
                     0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,
    WRITE_COMMAND_8, 0xC8,
    WRITE_BYTES, 32,
    0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
    0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
    0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
    0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
    WRITE_COMMAND_8, 0xD0,
    WRITE_BYTES, 5, 0x04, 0x06, 0x6B, 0x0F, 0x00,
    WRITE_C8_D16, 0xD7, 0x00, 0x30,
    WRITE_C8_D8,  0xE6, 0x14,
    WRITE_C8_D8,  0xDE, 0x01,
    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 5, 0x03, 0x13, 0xEF, 0x35, 0x35,
    WRITE_COMMAND_8, 0xC1,
    WRITE_BYTES, 3, 0x14, 0x15, 0xC0,
    WRITE_C8_D16, 0xC2, 0x06, 0x3A,
    WRITE_C8_D16, 0xC4, 0x72, 0x12,
    WRITE_C8_D8,  0xBE, 0x00,
    WRITE_C8_D8,  0xDE, 0x02,
    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3, 0x00, 0x02, 0x00,
    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3, 0x01, 0x02, 0x00,
    WRITE_C8_D8,  0xDE, 0x00,
    WRITE_C8_D8,  0x35, 0x00,
    WRITE_C8_D8,  0x3A, 0x05,
    WRITE_COMMAND_8, 0x2A,
    WRITE_BYTES, 4, 0x00, 0x22, 0x00, 0xCD,
    WRITE_COMMAND_8, 0x2B,
    WRITE_BYTES, 4, 0x00, 0x00, 0x01, 0x3F,
    WRITE_C8_D8,  0xDE, 0x02,
    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3, 0x00, 0x02, 0x00,
    WRITE_C8_D8,  0xDE, 0x00,
    WRITE_C8_D8,  0x36, 0x00,
    WRITE_COMMAND_8, 0x21,
    END_WRITE,
    DELAY, 10,
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,
    END_WRITE
  };
  bus->batchOperation(init_operations, sizeof(init_operations));
}

// ---------- Seitentitel und Tab-Leiste ----------
void drawTitle() {                         // nur Hochformat, Seiten GERAET / FOOT / SYS
  if (landscape || page == 0) return;
  const char* titles[4] = {"", TR("GERÄTE", "DEVICES"), "FOOT POD", "SYSTEM"};
  put(120, 0, 6, 2, C_FG, C_BG, titles[page], 14, 1);
}

void drawTabs() {                          // nur Anzeige: wo bin ich? (Wechsel durch Wischen)
  const char* names[4] = {"LIVE", TR("GERÄT", "DEVICE"), "FOOT", "SYS"};
  const int tw4 = W / 4;
  for (int i = 0; i < 4; i++) {
    bool act = (i == page);
    if (!changed(4 + i, act ? 1 : 2)) continue;
    int x = i * tw4;
    gfx->fillRect(x, TAB_Y, tw4, TAB_H, act ? C_ACC : C_TAB);
    char cs[16];
    toCp437(names[i], cs, sizeof cs);
    int tw = (int)strlen(cs) * 6;
    rawText(x + (tw4 - tw) / 2, TAB_Y + (TAB_H - 8) / 2, 1, act ? C_BG : C_FG, act ? C_ACC : C_TAB, names[i]);
  }
  if (needFull) gfx->drawFastHLine(0, TAB_Y - 1, W, C_LINE);
}

// ---------- Seite LIVE ----------
void indicator(int id, int x, int y, int w, const char* label, const char* status, uint16_t col) {
  uint32_t k = fnv(label) ^ (fnv(status) << 1) ^ ((uint32_t)col << 8);
  if (!changed(id, k)) return;
  card(x, y, w, 36, C_CARD);
  gfx->fillCircle(x + 12, y + 18, 7, col);
  rawText(x + 24, y + 8, 1, C_FG, C_CARD, label);
  rawText(x + 24, y + 20, 1, col, C_CARD, status);
}

void liveStatus(int idL, int idU, int xL, int xU, int y, int w) {
  uint16_t cl; const char* sl;
  if (S.ble == 2)                  { cl = C_OK;   sl = TR("verbunden", "connected"); }
  else if (S.scanning)             { cl = C_WARN; sl = TR("sucht...", "searching..."); }
  else if (S.hasSaved && S.autoOn) { cl = C_WARN; sl = TR("wartet", "waiting"); }
  else                             { cl = C_BAD;  sl = TR("getrennt", "disconnected"); }
  indicator(idL, xL, y, w, TR("LAUFBAND", "TREADMILL"), sl, cl);

  uint16_t cu; const char* su;
  if (!S.fpOn)            { cu = C_DIM;  su = TR("aus", "off"); }
  else if (!S.fpClients)  { cu = C_WARN; su = TR("wartet", "waiting"); }
  else if (S.fpSub)       { cu = C_OK;   su = TR("sendet", "sending"); }
  else                    { cu = C_WARN; su = TR("verbunden", "connected"); }
  indicator(idU, xU, y, w, TR("UHR", "WATCH"), su, cu);
}

// grosse Zahl mit getrennter Breiten-/Hoehenskalierung
void putBig(int id, int x, int y, uint8_t sx, uint8_t sy, uint16_t fg, uint16_t bg, const char* s, int chars) {
  char buf[16];
  int len = (int)strlen(s);
  if (chars > 14) chars = 14;
  if (len > chars) len = chars;
  memset(buf, ' ', chars);
  memcpy(buf + (chars - len), s, len);                   // rechtsbuendig im Feld
  buf[chars] = 0;
  uint32_t k = fnv(buf) ^ ((uint32_t)fg << 16) ^ bg ^ ((uint32_t)sx << 24) ^ ((uint32_t)sy << 28);
  if (!changed(id, k)) return;
  gfx->setTextSize(sx, sy);
  gfx->setTextColor(fg, bg);
  gfx->setCursor(x, y);
  gfx->print(buf);
}

void pageLive() {
  const bool ok = S.liveFresh;
  char vN[16], vP[16], vD[16], vS[16], vZ[16];
  if (ok) snprintf(vN, sizeof vN, "%.1f", S.speed); else snprintf(vN, sizeof vN, "--");
  if (ok && S.speed > 0.1f) { int ps = (int)(3600.0f / S.speed + 0.5f); snprintf(vP, sizeof vP, "%d:%02d", ps / 60, ps % 60); }
  else snprintf(vP, sizeof vP, "--");
  if (ok && S.dist >= 0) snprintf(vD, sizeof vD, "%.2f", S.dist / 1000.0); else snprintf(vD, sizeof vD, "--");
  if (ok && S.incl > -900) snprintf(vS, sizeof vS, "%.1f", S.incl); else snprintf(vS, sizeof vS, "--");
  if (ok && S.timeS >= 0) snprintf(vZ, sizeof vZ, "%02d:%02d:%02d", S.timeS / 3600, (S.timeS / 60) % 60, S.timeS % 60);
  else snprintf(vZ, sizeof vZ, "--:--:--");
  const uint16_t cNum = (ok && S.speed > 0.1f) ? C_ACC : C_DIM;
  const uint16_t cVal = ok ? C_FG : C_DIM;

  if (!landscape) {
    if (needFull) {
      rawText(124, 102, 2, C_DIM, C_BG, "km/h");                       // Einheit neben der Zahl
      card(3, 128, 82, 54, C_CARD);   rawText(9, 134, 1, C_DIM, C_CARD, "Pace min/km");
      card(87, 128, 82, 54, C_CARD);  rawText(93, 134, 1, C_DIM, C_CARD, TR("Distanz km", "Distance km"));
      card(3, 188, 166, 48, C_CARD);  rawText(9, 194, 1, C_DIM, C_CARD, TR("Zeit h:min:s", "Time h:min:s"));
      card(3, 242, 166, 36, C_CARD);  rawText(9, 256, 1, C_DIM, C_CARD, TR("Steigung %", "Incline %"));
    }
    liveStatus(21, 22, 3, 87, 10, 82);
    putBig(10, 2, 54, 5, 8, cNum, C_BG, vN, 4);                        // grosses Tempo (30 x 64 Pixel pro Ziffer)
    put(11, 9, 158, 3, cVal, C_CARD, vP, 4);
    put(12, 93, 158, 3, cVal, C_CARD, vD, 4);
    put(13, 9, 208, 3, cVal, C_CARD, vZ, 8);
    put(14, 94, 250, 2, cVal, C_CARD, vS, 6, 2);
  } else {
    if (needFull) {
      rawText(156, 92, 2, C_DIM, C_BG, "km/h");
      card(210, 44, 98, 30, C_CARD);   rawText(216, 47, 1, C_DIM, C_CARD, "Pace min/km");
      card(210, 78, 98, 30, C_CARD);   rawText(216, 81, 1, C_DIM, C_CARD, TR("Distanz km", "Distance km"));
      card(8, 112, 146, 30, C_CARD);   rawText(14, 123, 1, C_DIM, C_CARD, TR("Zeit", "Time"));          // untere Reihe: gleich gross,
      card(162, 112, 146, 30, C_CARD); rawText(168, 123, 1, C_DIM, C_CARD, TR("Steigung %", "Incline %"));  // gleiche Hoehe, buendig
    }
    liveStatus(21, 22, 8, 158, 4, 146);
    putBig(10, 8, 44, 6, 8, cNum, C_BG, vN, 4);                        // grosses Tempo (36 x 64 Pixel pro Ziffer)
    put(11, 216, 57, 2, cVal, C_CARD, vP, 7);
    put(12, 216, 91, 2, cVal, C_CARD, vD, 7);
    put(13, 48, 119, 2, cVal, C_CARD, vZ, 8);
    put(14, 242, 119, 2, cVal, C_CARD, vS, 4, 2);
  }
}

// ---------- Seite GERAET ----------
int devRows()  { return landscape ? 3 : 4; }
int devY0()    { return landscape ? 50 : 74; }
int devStep()  { return landscape ? 34 : 40; }
int devH()     { return landscape ? 30 : 36; }
int devW()     { return landscape ? 304 : 166; }
int devX()     { return landscape ? 8 : 3; }

void pageDev() {
  const bool conn = S.ble == 2, scan = S.scanning;
  const bool canSearch = !scan;
  const uint16_t cS = canSearch ? C_ACC : C_OFF, fS = canSearch ? C_BG : C_DIM;
  const uint16_t cT = conn ? C_BAD : C_OFF, fT = conn ? C_FG : C_DIM;
  const char* tScan = scan ? TR("SUCHE..", "SCAN..") : TR("SUCHEN", "SCAN");
  char b[64];
  if (!landscape) {
    button(10, 1, 28, 85, 30, tScan, cS, fS, 2);
    button(11, 87, 28, 84, 30, TR("TRENN.", "DISC."), cT, fT, 2);
    button(13, 3, 238, 80, 24, TR("AUF", "UP"), C_CARD, C_FG, 2);
    button(14, 89, 238, 80, 24, TR("AB", "DOWN"), C_CARD, C_FG, 2);
  } else {
    button(10, 8, 8, 76, 28, tScan, cS, fS, 2);
    button(11, 88, 8, 76, 28, TR("TRENN.", "DISC."), cT, fT, 2);
    button(13, 166, 8, 70, 28, TR("AUF", "UP"), C_CARD, C_FG, 2);
    button(14, 240, 8, 66, 28, TR("AB", "DOWN"), C_CARD, C_FG, 2);
  }

  if (S.hasSaved) snprintf(b, sizeof b, "%s %.14s", TR("Gespeichert:", "Saved:"), S.savedName[0] ? S.savedName : S.savedAddr);
  else snprintf(b, sizeof b, "%s", TR("Kein Gerät gespeichert", "No device saved"));
  if (!landscape) put(12, 10, 63, 1, C_DIM, C_BG, b, 26);
  else put(12, 10, 40, 1, C_DIM, C_BG, b, 48);

  const int rows = devRows(), y0 = devY0(), st = devStep(), dh = devH(), dw = devW(), dx = devX();
  if (devScroll > S.devCount - 1) devScroll = max(0, S.devCount - 1);
  for (int r = 0; r < rows; r++) {
    int y = y0 + r * st;
    int i = devScroll + r;
    if (i >= S.devCount) {
      if (changed(30 + r, 0xE0E0u)) gfx->fillRect(0, y, W, dh, C_BG);
      continue;
    }
    const UiDev& d = S.devs[i];
    const bool hi = d.saved || d.connected;
    const uint16_t bg = hi ? C_CARD_HI : C_CARD;
    uint32_t k = fnv(d.name) ^ (fnv(d.addr) << 1) ^ (d.ftms ? 1u : 0u) ^ (d.saved ? 2u : 0u) ^ (d.connected ? 4u : 0u) ^ ((uint32_t)g_lang << 8);
    if (changed(30 + r, k)) {
      card(dx, y, dw, dh, bg);
      rawTextN(dx + 6, y + (landscape ? 3 : 4), 2, d.ftms ? C_OK : C_FG, bg,
               d.name[0] ? d.name : TR("(ohne Name)", "(no name)"), landscape ? 24 : 12);     // zu lange Namen abschneiden
      keys[40 + r] = 0xFFFFFFFFu;     // RSSI-Text neu zeichnen
    }
    char s[32];
    snprintf(s, sizeof s, "%s %ddBm", d.addr, d.rssi);
    put(40 + r, dx + 6, y + (landscape ? 20 : 24), 1, C_DIM, bg, s, 26);
  }

  if (!landscape) {
    if (S.devCount > 0) snprintf(b, sizeof b, TR("%d Geräte, ab Nr. %d", "%d devices, from no. %d"), S.devCount, devScroll + 1);
    else snprintf(b, sizeof b, "%s", TR("keine Geräte - SUCHEN tippen", "no devices - tap SCAN"));
    put(15, 10, 266, 1, C_DIM, C_BG, b, 26, 1);
    put(16, 10, 276, 1, C_DIM, C_BG, S.msg, 26);
  }
}

// ---------- Seite FOOT ----------
const int CAD_Y = 198, SIM_Y = 244;           // Hochformat: obere Kante der Plus/Minus-Zeilen (Hoehe 30)
const int LCAD_Y = 20, LSIM_Y = 80;           // Querformat (Hoehe 34)

void pageFoot() {
  const char* labelsP[5] = {"Foot Pod", TR("Kadenz", "Cadence"), TR("Schrittl.", "Stride"), TR("Distanz", "Distance"), TR("Lauf/Geh", "Run/Walk")};
  char b[16];
  for (int i = 0; i < 5; i++) {
    bool on = (i == 0) ? S.fpOn : (((S.mask >> (i - 1)) & 1) != 0);
    if (!landscape) {
      int y = 30 + i * 31;
      if (needFull) { card(3, y, 166, 28, C_CARD); rawText(9, y + 6, 2, C_FG, C_CARD, labelsP[i]); }
      pill(60 + i, 124, y + 5, on, C_CARD);
    } else {
      int y = 4 + i * 28;
      if (needFull) { card(8, y, 150, 24, C_CARD); rawText(14, y + 4, 2, C_FG, C_CARD, labelsP[i]); }
      pill(60 + i, 114, y + 3, on, C_CARD);
    }
  }
  if (!landscape) {
    if (needFull) {
      rawText(6, CAD_Y - 11, 1, C_DIM, C_BG, TR("Kadenz (Schritte/min)", "Cadence (steps/min)"));
      rawText(6, SIM_Y - 11, 1, C_DIM, C_BG, TR("Test-Tempo (km/h)", "Test speed (km/h)"));
    }
    button(70, 3, CAD_Y, 44, 30, "-", C_CARD, C_FG, 3);
    button(71, 125, CAD_Y, 44, 30, "+", C_CARD, C_FG, 3);
    button(73, 3, SIM_Y, 44, 30, "-", C_CARD, C_FG, 3);
    button(74, 125, SIM_Y, 44, 30, "+", C_CARD, C_FG, 3);
  } else {
    if (needFull) {
      rawText(172, 8, 1, C_DIM, C_BG, TR("Kadenz (Schritte/min)", "Cadence (steps/min)"));
      rawText(172, 68, 1, C_DIM, C_BG, TR("Test-Tempo (km/h)", "Test speed (km/h)"));
    }
    button(70, 168, LCAD_Y, 40, 34, "-", C_CARD, C_FG, 3);
    button(71, 272, LCAD_Y, 40, 34, "+", C_CARD, C_FG, 3);
    button(73, 168, LSIM_Y, 40, 34, "-", C_CARD, C_FG, 3);
    button(74, 272, LSIM_Y, 40, 34, "+", C_CARD, C_FG, 3);
  }
  if (S.cadManual > 0) snprintf(b, sizeof b, "%d", S.cadManual); else snprintf(b, sizeof b, "auto");
  if (!landscape) put(72, 50, CAD_Y + 8, 2, C_ACC, C_BG, b, 6, 1);
  else put(72, 210, LCAD_Y + 9, 2, C_ACC, C_BG, b, 5, 1);
  if (S.sim >= 0) snprintf(b, sizeof b, "%.1f", S.sim); else snprintf(b, sizeof b, "%s", TR("aus", "off"));
  if (!landscape) put(75, 50, SIM_Y + 8, 2, S.sim >= 0 ? C_WARN : C_DIM, C_BG, b, 6, 1);
  else put(75, 210, LSIM_Y + 9, 2, S.sim >= 0 ? C_WARN : C_DIM, C_BG, b, 5, 1);
}

// ---------- Seite SYS ----------
void pageSys() {
  char b[64];
  const char* ws = S.webState == 1 ? TR("Webserver startet...", "Web server starting...")
                                    : (S.webState == 2 ? TR("Webserver läuft:", "Web server running:") : TR("Webserver aus", "Web server off"));
  const char* fgt = forgetArmed ? TR("SICHER? NOCHMAL TIPPEN", "SURE? TAP AGAIN") : TR("GERÄT VERGESSEN", "FORGET DEVICE");
  const char* prf = profileArmed ? TR("SICHER? NEUSTART", "SURE? RESTART") : TR("PROFIL WECHSELN", "SWITCH PROFILE");
  if (!landscape) {
    if (needFull) {
      rawText(10, 28, 1, C_DIM, C_BG, TR("Helligkeit", "Brightness"));
      card(3, 70, 166, 26, C_CARD);  rawText(9, 75, 2, C_FG, C_CARD, TR("Auto-Verb.", "Auto-conn."));
      card(3, 100, 166, 26, C_CARD); rawText(9, 105, 2, C_FG, C_CARD, TR("Webserver", "Web server"));
      card(3, 130, 166, 26, C_CARD); rawText(9, 135, 2, C_FG, C_CARD, TR("Waagrecht", "Landscape"));
    }
    button(80, 3, 38, 44, 28, "-", C_CARD, C_FG, 3);
    button(81, 125, 38, 44, 28, "+", C_CARD, C_FG, 3);
    snprintf(b, sizeof b, "%d%%", brightness);
    put(82, 50, 44, 2, C_ACC, C_BG, b, 6, 1);
    pill(83, 124, 74, S.autoOn, C_CARD);
    pill(86, 124, 104, S.webOn, C_CARD);
    pill(87, 124, 134, landscape, C_CARD);
    button(84, 3, 160, 166, 26, fgt, forgetArmed ? C_BAD : (S.hasSaved ? C_CARD : C_OFF), S.hasSaved ? C_FG : C_DIM, 1);
    button(85, 3, 190, 166, 26, prf, profileArmed ? C_BAD : C_CARD, C_FG, 1);
    put(101, 10, 222, 1, C_DIM, C_BG, ws, 26);
    for (int i = 0; i < 2; i++) {                       // WLAN-Text auf Zeilen verteilen
      char line[27] = {0};
      size_t off = (size_t)i * 26;
      if (S.webState == 2 && off < strlen(S.wifi)) strncpy(line, S.wifi + off, 26);
      put(102 + i, 10, 231 + i * 9, 1, C_FG, C_BG, line, 26);
    }
    if (S.webState == 2) snprintf(b, sizeof b, "http://%s.local", S.host); else b[0] = 0;
    put(104, 10, 249, 1, C_DIM, C_BG, b, 26);
    snprintf(b, sizeof b, TR("Heap frei %u KB, Lauf %lu min", "Heap free %u KB, up %lu min"), (unsigned)(ESP.getFreeHeap() / 1024), (unsigned long)(millis() / 60000));
    put(105, 10, 258, 1, C_DIM, C_BG, b, 26);
    snprintf(b, sizeof b, TR("Foot-Pod-Pakete: %lu", "Foot pod packets: %lu"), (unsigned long)S.fpSent);
    put(106, 10, 267, 1, C_DIM, C_BG, b, 26);
  } else {
    if (needFull) {
      rawText(10, 4, 1, C_DIM, C_BG, TR("Helligkeit", "Brightness"));
      card(8, 44, 150, 24, C_CARD);  rawText(14, 52, 1, C_FG, C_CARD, TR("Auto-Verbinden", "Auto-connect"));
      card(8, 72, 150, 24, C_CARD);  rawText(14, 80, 1, C_FG, C_CARD, TR("Webserver", "Web server"));
      card(8, 100, 150, 24, C_CARD); rawText(14, 108, 1, C_FG, C_CARD, TR("Waagrecht", "Landscape"));
    }
    button(80, 8, 14, 40, 26, "-", C_CARD, C_FG, 3);
    button(81, 118, 14, 40, 26, "+", C_CARD, C_FG, 3);
    snprintf(b, sizeof b, "%d%%", brightness);
    put(82, 56, 19, 2, C_ACC, C_BG, b, 5, 1);
    pill(83, 114, 47, S.autoOn, C_CARD);
    pill(86, 114, 75, S.webOn, C_CARD);
    pill(87, 114, 103, landscape, C_CARD);
    button(84, 168, 8, 138, 24, fgt, forgetArmed ? C_BAD : (S.hasSaved ? C_CARD : C_OFF), S.hasSaved ? C_FG : C_DIM, 1);
    button(85, 168, 36, 138, 24, prf, profileArmed ? C_BAD : C_CARD, C_FG, 1);
    put(101, 170, 66, 1, C_DIM, C_BG, ws, 24);
    for (int i = 0; i < 2; i++) {
      char line[25] = {0};
      size_t off = (size_t)i * 24;
      if (S.webState == 2 && off < strlen(S.wifi)) strncpy(line, S.wifi + off, 24);
      put(102 + i, 170, 75 + i * 9, 1, C_FG, C_BG, line, 24);
    }
    if (S.webState == 2) snprintf(b, sizeof b, "http://%s.local", S.host); else b[0] = 0;
    put(104, 170, 93, 1, C_DIM, C_BG, b, 24);
    snprintf(b, sizeof b, TR("Heap %u KB | %lu min", "Heap %u KB | up %lu min"), (unsigned)(ESP.getFreeHeap() / 1024), (unsigned long)(millis() / 60000));
    put(105, 170, 102, 1, C_DIM, C_BG, b, 24);
    snprintf(b, sizeof b, TR("FP-Pakete: %lu", "FP packets: %lu"), (unsigned long)S.fpSent);
    put(106, 170, 111, 1, C_DIM, C_BG, b, 24);
  }
}

// ---------- Rendern ----------
void render() {
  static int lastLang = -1;
  if (g_lang != lastLang) { lastLang = g_lang; needFull = true; }      // Sprache gewechselt: alles neu zeichnen
  if (needFull) gfx->fillScreen(C_BG);
  drawTitle();
  switch (page) {
    case 0: pageLive(); break;
    case 1: pageDev(); break;
    case 2: pageFoot(); break;
    default: pageSys(); break;
  }
  drawTabs();
  needFull = false;
}

// ---------- Aktionen ----------
void gotoPage(int p) {
  p = (p % 4 + 4) % 4;
  if (p == page) return;
  page = p; needFull = true; devScroll = 0; forgetArmed = false; profileArmed = false;
}

void doCadence(int dir) {
  if (dir < 0) actCadence(S.cadManual > 120 ? S.cadManual - 5 : 0);
  else actCadence(S.cadManual == 0 ? 120 : min(220, S.cadManual + 5));
}
void doSim(int dir) {
  if (dir < 0) actSim(S.sim <= 0 ? -1.0f : S.sim - 1.0f);
  else actSim(S.sim < 0 ? 6.0f : min(40.0f, S.sim + 1.0f));
}
void doBright(int dir) {
  brightness = dir < 0 ? max(10, brightness - 10) : min(100, brightness + 10);
  applyBrightness();
  uiPrefs.putUChar("bright", (uint8_t)brightness);
}
void doAuto() { if (S.hasSaved) actAuto(!S.autoOn); }
void doForget() {
  if (!S.hasSaved) return;
  if (!forgetArmed) { forgetArmed = true; armedAt = millis(); }
  else { forgetArmed = false; actForget(); }
}
void doProfile() {
  if (!profileArmed) { profileArmed = true; armedAt = millis(); }
  else { profileArmed = false; actRestart(); }
}
void doOrientation() {
  uiPrefs.putBool("land", !landscape);
  setOrientation(!landscape);
}

void tapDev(int x, int y) {
  if (!landscape) {
    if (inRect(x, y, 1, 28, 85, 30)) { if (!S.scanning) actScan(); return; }
    if (inRect(x, y, 87, 28, 84, 30)) { if (S.ble == 2) actDisconnect(); return; }
    if (inRect(x, y, 3, 238, 80, 24)) { devScroll = max(0, devScroll - devRows()); return; }
    if (inRect(x, y, 89, 238, 80, 24)) { if (devScroll + devRows() < S.devCount) devScroll += devRows(); return; }
  } else {
    if (inRect(x, y, 8, 8, 76, 28)) { if (!S.scanning) actScan(); return; }
    if (inRect(x, y, 88, 8, 76, 28)) { if (S.ble == 2) actDisconnect(); return; }
    if (inRect(x, y, 166, 8, 70, 28)) { devScroll = max(0, devScroll - devRows()); return; }
    if (inRect(x, y, 240, 8, 66, 28)) { if (devScroll + devRows() < S.devCount) devScroll += devRows(); return; }
  }
  for (int r = 0; r < devRows(); r++) {
    if (inRect(x, y, devX(), devY0() + r * devStep(), devW(), devH())) {
      int i = devScroll + r;
      if (i < S.devCount && S.ble != 2) actConnect(S.devs[i].idx);
    }
  }
}

void tapFoot(int x, int y) {
  for (int i = 0; i < 5; i++) {
    bool hit = !landscape ? inRect(x, y, 3, 30 + i * 31, 166, 28) : inRect(x, y, 8, 4 + i * 28, 150, 24);
    if (hit) { if (i == 0) actFootpod(!S.fpOn); else actMask(S.mask ^ (1 << (i - 1))); return; }
  }
  if (!landscape) {
    if (inRect(x, y, 3, CAD_Y, 44, 30)) doCadence(-1);
    else if (inRect(x, y, 125, CAD_Y, 44, 30)) doCadence(1);
    else if (inRect(x, y, 3, SIM_Y, 44, 30)) doSim(-1);
    else if (inRect(x, y, 125, SIM_Y, 44, 30)) doSim(1);
  } else {
    if (inRect(x, y, 168, LCAD_Y, 40, 34)) doCadence(-1);
    else if (inRect(x, y, 272, LCAD_Y, 40, 34)) doCadence(1);
    else if (inRect(x, y, 168, LSIM_Y, 40, 34)) doSim(-1);
    else if (inRect(x, y, 272, LSIM_Y, 40, 34)) doSim(1);
  }
}

void tapSys(int x, int y) {
  if (!landscape) {
    if (inRect(x, y, 3, 38, 44, 28)) doBright(-1);
    else if (inRect(x, y, 125, 38, 44, 28)) doBright(1);
    else if (inRect(x, y, 3, 70, 166, 26)) doAuto();
    else if (inRect(x, y, 3, 100, 166, 26)) actWeb(!S.webOn);
    else if (inRect(x, y, 3, 130, 166, 26)) doOrientation();
    else if (inRect(x, y, 3, 160, 166, 26)) doForget();
    else if (inRect(x, y, 3, 190, 166, 26)) doProfile();
  } else {
    if (inRect(x, y, 8, 14, 40, 26)) doBright(-1);
    else if (inRect(x, y, 118, 14, 40, 26)) doBright(1);
    else if (inRect(x, y, 8, 44, 150, 24)) doAuto();
    else if (inRect(x, y, 8, 72, 150, 24)) actWeb(!S.webOn);
    else if (inRect(x, y, 8, 100, 150, 24)) doOrientation();
    else if (inRect(x, y, 168, 8, 138, 24)) doForget();
    else if (inRect(x, y, 168, 36, 138, 24)) doProfile();
  }
}

void onTap(int x, int y) {
  if (y >= TAB_Y) { gotoPage(x / (W / 4)); return; }   // Tippen auf die Leiste geht auch, Wischen ist aber einfacher
  switch (page) {
    case 1: tapDev(x, y); break;
    case 2: tapFoot(x, y); break;
    case 3: tapSys(x, y); break;
  }
}

}  // namespace

// ---------- oeffentliche Funktionen ----------
void uiBegin() {
  // Reihenfolge wie im funktionierenden Waveshare-Beispiel: CS-Pins hoch, SPI starten, Display, Beleuchtung
  Serial.println("[UI] 1: CS-Pins auf HIGH");
  pinMode(PIN_SD_CS, OUTPUT);  digitalWrite(PIN_SD_CS, HIGH);    // TF-Slot am gemeinsamen SPI-Bus ruhig halten
  pinMode(PIN_LCD_CS, OUTPUT); digitalWrite(PIN_LCD_CS, HIGH);

  Serial.println("[UI] 2: SPI.begin");
  SPI.begin(PIN_LCD_SCK, PIN_SD_MISO, PIN_LCD_MOSI, PIN_SD_CS);  // Bus teilen sich Display und TF-Slot

  bus = new Arduino_HWSPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_MOSI);
  gfx = new Arduino_ST7789(bus, PIN_LCD_RST, 0 /* Rotation */, false /* IPS */, LCD_W, LCD_H,
                           LCD_COL_OFFSET, LCD_ROW_OFFSET, LCD_COL_OFFSET, LCD_ROW_OFFSET);
  Serial.println("[UI] 3: gfx->begin()");
  bool ok = gfx->begin();
  Serial.printf("[UI]    begin() %s\n", ok ? "ok" : "FEHLER");
  lcdRegInit();
  gfx->setRotation(0);
  Serial.println("[UI] 4: Panel-Registerfolge gesendet");

  C_BG = gfx->color565(8, 10, 14);      C_FG = gfx->color565(235, 238, 242);
  C_DIM = gfx->color565(130, 138, 150); C_ACC = gfx->color565(0, 200, 140);
  C_OK = gfx->color565(60, 210, 110);   C_WARN = gfx->color565(240, 190, 40);
  C_BAD = gfx->color565(220, 70, 70);   C_CARD = gfx->color565(28, 32, 40);
  C_CARD_HI = gfx->color565(26, 62, 56); C_OFF = gfx->color565(60, 64, 72);
  C_TAB = gfx->color565(24, 28, 34);    C_LINE = gfx->color565(50, 56, 66);
  gfx->fillScreen(gfx->color565(0, 0, 0));

  uiPrefs.begin("ui", false);
  brightness = constrain((int)uiPrefs.getUChar("bright", 50), 10, 100);
  setOrientation(uiPrefs.getBool("land", false));       // gespeicherte Ausrichtung
  ledcAttach(PIN_LCD_BL, 5000, 8);
  applyBrightness();
  Serial.printf("[UI] 5: Beleuchtung an (%d%%), %s\n", brightness, landscape ? "waagrecht" : "hochkant");

  // Selbsttest bei eingeschalteter Beleuchtung: Farbbalken Rot, Gruen, Blau und Text (ca. 1 s)
  gfx->fillRect(0, 0, W, H / 3, gfx->color565(255, 0, 0));
  gfx->fillRect(0, H / 3, W, H / 3, gfx->color565(0, 255, 0));
  gfx->fillRect(0, 2 * (H / 3), W, H - 2 * (H / 3), gfx->color565(0, 0, 255));
  rawText(W / 2 - 60, H / 2 - 8, 2, C_FG, C_BG, "Display OK");
  Serial.println("[UI] 6: Farbbalken gezeichnet");
  delay(1000);
  gfx->fillScreen(C_BG);

  bool tp = touchBegin();
  Serial.printf("[UI] 7: Touch-Controller %s\n", tp ? "gefunden" : "NICHT gefunden (I2C-Antwort fehlt)");
}

void uiSplash(const char* line) {
  if (!gfx) return;
  int y0 = landscape ? 40 : 100;
  gfx->fillRect(0, y0, W, 70, C_BG);
  rawText(20, y0 + 4, 2, C_ACC, C_BG, TR("Laufband", "Treadmill"));
  rawText(20, y0 + 24, 2, C_ACC, C_BG, "Bridge");
  rawText(20, y0 + 50, 1, C_DIM, C_BG, line);
  needFull = true;
}

// Profilauswahl beim Start: blockiert, bis ein Name angetippt wurde. Namen stehen in include/config.h.
int uiChooseProfile(int last) {
  if (last != 2) last = 1;
  gfx->fillScreen(C_BG);
  int bx[2], by[2], bw, bh;
  if (!landscape) { bw = 156; bh = 100; bx[0] = bx[1] = 8; by[0] = 62; by[1] = 174; rawText(20, 26, 2, C_FG, C_BG, TR("Wer läuft:", "Who runs:")); }
  else            { bw = 148; bh = 100; bx[0] = 8; bx[1] = 164; by[0] = by[1] = 44; rawText(20, 14, 2, C_FG, C_BG, TR("Wer läuft:", "Who runs:")); }
  for (int p = 0; p < 2; p++) {
    gfx->fillRoundRect(bx[p], by[p], bw, bh, 12, C_ACC);
    const char* nm = profileName(p + 1);
    char cs[32];
    toCp437(nm, cs, sizeof cs);
    uint8_t sz = strlen(cs) <= 8 ? 3 : 2;
    int tw = (int)strlen(cs) * 6 * sz;
    rawText(bx[p] + (bw - tw) / 2, by[p] + (bh - 8 * sz) / 2, sz, C_BG, C_ACC, nm);
  }
  Serial.println("[UI] Profilauswahl wartet auf Touch");

  const uint32_t t0 = millis();
  bool wasDown = false;
  for (;;) {
    int x = 0, y = 0;
    bool down = readTouch(x, y);
    if (down && !wasDown) {
      for (int p = 0; p < 2; p++) {
        if (inRect(x, y, bx[p], by[p], bw, bh)) { needFull = true; return p + 1; }
      }
    }
    wasDown = down;
    if (PROFILE_AUTOSTART_S > 0 && millis() - t0 > (uint32_t)PROFILE_AUTOSTART_S * 1000u) { needFull = true; return last; }
    delay(10);
  }
}

void uiLoop() {
  static bool wasDown = false, force = true;
  static int sx = 0, sy = 0, lx = 0, ly = 0;
  static uint32_t tDown = 0, lastDraw = 0;
  const uint32_t now = millis();

  int x = 0, y = 0;
  bool down = readTouch(x, y);
  if (down) {
    if (!wasDown) { sx = x; sy = y; tDown = now; }
    lx = x; ly = y;
  } else if (wasDown) {                                // Finger losgelassen: Tippen oder Wischen auswerten
    int dx = lx - sx, dy = ly - sy;
    if (abs(dx) >= SWIPE_MIN && abs(dx) * 2 > abs(dy) * 3) {
      gotoPage(dx < 0 ? page + 1 : page - 1);          // nach links wischen = naechste Seite
    } else if (page == 1 && abs(dy) >= 24 && abs(dy) * 2 > abs(dx) * 3) {
      int rows = max(1, abs(dy) / devStep());          // Geraeteliste scrollen
      devScroll = dy < 0 ? min(max(0, S.devCount - 1), devScroll + rows) : max(0, devScroll - rows);
    } else if (abs(dx) < 14 && abs(dy) < 14 && now - tDown < 900) {
      onTap(sx, sy);
    }
    force = true;
  }
  wasDown = down;

  if ((forgetArmed || profileArmed) && now - armedAt > 3000) { forgetArmed = false; profileArmed = false; force = true; }

  if (force || now - lastDraw >= 250) {
    force = false;
    lastDraw = now;
    appSnapshot(S);
    render();
    static bool first = true;
    if (first) { first = false; Serial.println("[UI] erste Seite gezeichnet"); }
  }
  delay(8);
}
