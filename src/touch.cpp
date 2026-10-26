#include "touch.h"
#include <Wire.h>
#include "pins.h"

static volatile bool g_int = false;
static void IRAM_ATTR touchIsr() { g_int = true; }

static bool readReg(uint8_t reg, uint8_t* buf, size_t n) {
  Wire.beginTransmission(TP_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint16_t)TP_ADDR, (size_t)n) != n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

bool touchBegin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  Wire.setTimeOut(50);                       // I2C must never block for long
  pinMode(PIN_TP_RST, OUTPUT);
  digitalWrite(PIN_TP_RST, LOW);
  delay(100);
  digitalWrite(PIN_TP_RST, HIGH);
  delay(300);
  pinMode(PIN_TP_INT, INPUT_PULLUP);
  attachInterrupt(PIN_TP_INT, touchIsr, FALLING);
  uint8_t id[3] = {0};
  return readReg(0x08, id, 3);               // ID register
}

bool touchPoll(int& x, int& y) {
  static bool pressed = false;
  static uint32_t lastEvent = 0;
  static int lx = 0, ly = 0;
  const uint32_t now = millis();

  if (g_int) {                               // controller reports new data
    g_int = false;
    uint8_t d[14] = {0};
    if (readReg(0x01, d, 14)) {
      if (d[1] > 0) {                        // d[1] = number of touches
        int rx = ((d[2] & 0x0F) << 8) | d[3];
        int ry = ((d[4] & 0x0F) << 8) | d[5];
        int tx = rx, ty = ry;
        if (TOUCH_SWAP_XY) { int t = tx; tx = ty; ty = t; }
        if (TOUCH_FLIP_X) tx = LCD_W - 1 - tx;
        if (TOUCH_FLIP_Y) ty = LCD_H - 1 - ty;
        lx = constrain(tx, 0, LCD_W - 1);
        ly = constrain(ty, 0, LCD_H - 1);
        pressed = true;
        lastEvent = now;
      } else {
        pressed = false;
      }
    }
  }
  if (pressed && now - lastEvent > 150) pressed = false;   // catch a release without a report
  if (pressed) { x = lx; y = ly; }
  return pressed;
}
