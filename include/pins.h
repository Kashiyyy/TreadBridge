#pragma once
// Waveshare ESP32-C6-Touch-LCD-1.47 (docs.waveshare.com/ESP32-C6-Touch-LCD-1.47)
// Display JD9853 (ST7789 command set), 172 x 320, 4-wire SPI. The SPI bus is shared with the TF slot.
#define PIN_LCD_SCK    1
#define PIN_LCD_MOSI   2
#define PIN_SD_MISO    3      // MISO of the shared SPI bus (TF slot only)
#define PIN_LCD_CS     14
#define PIN_LCD_DC     15
#define PIN_LCD_RST    22
#define PIN_LCD_BL     23     // backlight (PWM)
#define PIN_SD_CS      4      // TF slot: keep CS high so it does not disturb the shared bus

#define LCD_W          172
#define LCD_H          320
#define LCD_COL_OFFSET 34
#define LCD_ROW_OFFSET 0

// Touch AXS5106L (I2C, address 0x63)
#define PIN_I2C_SDA    18
#define PIN_I2C_SCL    19
#define PIN_TP_RST     20
#define PIN_TP_INT     21
#define TP_ADDR        0x63

// Touch orientation (portrait, USB at the bottom). If touch is mirrored or swapped,
// change these (0/1):
#define TOUCH_SWAP_XY  0
#define TOUCH_FLIP_X   1
#define TOUCH_FLIP_Y   0

// Landscape view: rotation direction of the display. If the picture is upside down (and touch
// does not match), swap 1 <-> 3 here.
#define LANDSCAPE_ROT  1
