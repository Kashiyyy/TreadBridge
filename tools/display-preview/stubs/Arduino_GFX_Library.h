#pragma once
// Minimal software implementation of the Arduino_GFX calls used by ui.cpp.
// Text, rectangles, rounded rectangles and circles follow the same algorithms as Adafruit_GFX /
// Arduino_GFX (5x7 classic font in a 6x8 cell, integer text scaling), so the output matches the panel.
#include <Arduino.h>
#include <vector>
extern const unsigned char font[];

enum { BEGIN_WRITE = 0xF0, WRITE_COMMAND_8, END_WRITE, DELAY, WRITE_C8_D16, WRITE_C8_D8, WRITE_BYTES };
struct Arduino_DataBus { void batchOperation(const uint8_t*, size_t) {} };
struct Arduino_HWSPI : Arduino_DataBus { Arduino_HWSPI(int,int,int,int) {} };

class Arduino_GFX {
 public:
  int _w, _h;                       // current logical size
  int pw, ph;                       // physical (portrait) size
  std::vector<uint16_t> fb;
  int cx = 0, cy = 0, ts = 1, tsy = 1;
  uint16_t fg = 0xFFFF, bg = 0x0000;
  Arduino_GFX(int w, int h) : _w(w), _h(h), pw(w), ph(h), fb((size_t)std::max(w, h) * std::max(w, h), 0) {}
  virtual ~Arduino_GFX() {}
  bool begin() { return true; }
  int width() const { return _w; }
  int height() const { return _h; }
  void setRotation(uint8_t r) { if (r & 1) { _w = ph; _h = pw; } else { _w = pw; _h = ph; } }
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
  void px(int x, int y, uint16_t c) { if (x >= 0 && y >= 0 && x < _w && y < _h) fb[(size_t)y * _w + x] = c; }
  void fillRect(int x, int y, int w, int h, uint16_t c) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) px(i, j, c); }
  void fillScreen(uint16_t c) { fillRect(0, 0, _w, _h, c); }
  void drawFastHLine(int x, int y, int w, uint16_t c) { fillRect(x, y, w, 1, c); }
  void drawFastVLine(int x, int y, int h, uint16_t c) { fillRect(x, y, 1, h, c); }
  void fillCircleHelper(int x0, int y0, int r, uint8_t corners, int delta, uint16_t color) {
    int f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r, px_ = x, py_ = y;
    delta++;
    while (x < y) {
      if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
      x++; ddF_x += 2; f += ddF_x;
      if (x < (y + 1)) {
        if (corners & 1) drawFastVLine(x0 + x, y0 - y, 2 * y + delta, color);
        if (corners & 2) drawFastVLine(x0 - x, y0 - y, 2 * y + delta, color);
      }
      if (y != py_) {
        if (corners & 1) drawFastVLine(x0 + py_, y0 - px_, 2 * px_ + delta, color);
        if (corners & 2) drawFastVLine(x0 - py_, y0 - px_, 2 * px_ + delta, color);
        py_ = y;
      }
      px_ = x;
    }
  }
  void fillCircle(int x0, int y0, int r, uint16_t c) { drawFastVLine(x0, y0 - r, 2 * r + 1, c); fillCircleHelper(x0, y0, r, 3, 0, c); }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
    int mr = std::min(w, h) / 2; if (r > mr) r = mr;
    fillRect(x + r, y, w - 2 * r, h, c);
    fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, c);
    fillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, c);
  }
  void setTextSize(uint8_t s) { ts = s; tsy = s; }
  void setTextSize(uint8_t sx, uint8_t sy) { ts = sx; tsy = sy; }
  void setTextColor(uint16_t f, uint16_t b) { fg = f; bg = b; }
  void setCursor(int x, int y) { cx = x; cy = y; }
  void drawChar(int x, int y, unsigned char ch, uint16_t f, uint16_t b, int s, int sy) {
    for (int i = 0; i < 5; i++) {
      uint8_t line = font[ch * 5 + i];
      for (int j = 0; j < 8; j++, line >>= 1) {
        if (line & 1) fillRect(x + i * s, y + j * sy, s, sy, f);
        else if (b != f) fillRect(x + i * s, y + j * sy, s, sy, b);
      }
    }
    if (b != f) fillRect(x + 5 * s, y, s, 8 * sy, b);
  }
  void print(const char* s) {
    for (; *s; s++) {
      unsigned char c = (unsigned char)*s;
      if (c == '\n') { cx = 0; cy += 8 * tsy; continue; }
      if (c == '\r') continue;
      if (cx + 6 * ts > _w) { cx = 0; cy += 8 * tsy; }
      drawChar(cx, cy, c, fg, bg, ts, tsy);
      cx += 6 * ts;
    }
  }
};
struct Arduino_ST7789 : Arduino_GFX {
  Arduino_ST7789(Arduino_DataBus*, int, uint8_t, bool, int w, int h, int, int, int, int) : Arduino_GFX(w, h) {}
};
