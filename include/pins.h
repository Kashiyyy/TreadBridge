#pragma once
// Waveshare ESP32-C6-Touch-LCD-1.47 (docs.waveshare.com/ESP32-C6-Touch-LCD-1.47)
// Display JD9853 (ST7789-Befehlssatz), 172 x 320, 4-Draht-SPI. Der SPI-Bus wird mit dem TF-Slot geteilt.
#define PIN_LCD_SCK    1
#define PIN_LCD_MOSI   2
#define PIN_SD_MISO    3      // MISO des geteilten SPI-Busses (nur TF-Slot)
#define PIN_LCD_CS     14
#define PIN_LCD_DC     15
#define PIN_LCD_RST    22
#define PIN_LCD_BL     23     // Hintergrundbeleuchtung (PWM)
#define PIN_SD_CS      4      // TF-Slot: CS hochhalten, damit er den geteilten Bus nicht stoert

#define LCD_W          172
#define LCD_H          320
#define LCD_COL_OFFSET 34
#define LCD_ROW_OFFSET 0

// Touch AXS5106L (I2C, Adresse 0x63)
#define PIN_I2C_SDA    18
#define PIN_I2C_SCL    19
#define PIN_TP_RST     20
#define PIN_TP_INT     21
#define TP_ADDR        0x63

// Touch-Ausrichtung (Hochformat, USB unten). Reagiert Touch gespiegelt oder vertauscht,
// hier umstellen (0/1):
#define TOUCH_SWAP_XY  0
#define TOUCH_FLIP_X   1
#define TOUCH_FLIP_Y   0

// Querformat (waagrechte Ansicht): Drehrichtung des Displays. Steht das Bild auf dem Kopf (und der Touch
// passt nicht), hier 1 <-> 3 tauschen.
#define LANDSCAPE_ROT  1
