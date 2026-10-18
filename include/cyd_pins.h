#ifndef CYD_PINS_H
#define CYD_PINS_H
//
// ESP32-2432S028R CYD Pin Definitions
//
// Verified against hardware testing and schematic
//
// ====================================================
// TFT Display (ILI9341)
// ====================================================
#define TFT_BL      21
#define TFT_DC      2
#define TFT_CS      15
#define TFT_SCK     14
#define TFT_MOSI    13
#define TFT_MISO    12
// Not connected
#define TFT_RST     -1
// ====================================================
// Touch Controller (XPT2046)
// ====================================================
#define TOUCH_CS    33
#define TOUCH_IRQ   36
#define TOUCH_SCK   25
#define TOUCH_MOSI  32
#define TOUCH_MISO  39
// Raw calibration values
#define TOUCH_X_MIN 250
#define TOUCH_X_MAX 3760
#define TOUCH_Y_MIN 250
#define TOUCH_Y_MAX 3700
// ====================================================
// microSD Card
// ====================================================
#define SD_CS       5
#define SD_SCK      18
#define SD_MOSI     23
#define SD_MISO     19
// ====================================================
// Audio
// ====================================================
#define SPEAKER_PIN 26
// ====================================================
// Ambient Light Sensor
// ====================================================
// Ambient light sensor
//
// Schematic indicates GPIO34.
// Board testing has not confirmed a working ADC signal.
// Currently unused.
//
#define LIGHT_ADC 34
// ====================================================
// RGB Status LED
// Common-Anode
// LOW  = ON
// HIGH = OFF
// ====================================================
#define LED_RED      4
#define LED_GREEN   16
#define LED_BLUE    17
// ====================================================
// Expansion Connector
// ====================================================
#define CN1_PIN     27
// ====================================================
// Common Colors (RGB565)
// ====================================================
#define CYD_BLACK       0x0000
#define CYD_WHITE       0xFFFF
#define CYD_RED         0xF800
#define CYD_BRIGHTRED  0xF810
#define CYD_DARK_RED    0x8000
#define CYD_GREEN       0x07E0
#define CYD_DARK_GREEN  0x03E0
#define CYD_BLUE        0x001F
#define CYD_YELLOW      0xFFE0
#define CYD_CYAN        0x07FF
#define CYD_MAGENTA     0xF81F
#define CYD_ORANGE      0xFD20
#define CYD_GRAY        0x8410
#define CYD_PINK        0xF81F
#define CYD_BROWN       0xA145
#define CYD_PURPLE      0x8010
#define CYD_DARK_PURPLE 0x4008
#define CYD_NAVY        0x000F
#define CYD_LIGHT_GRAY  0xC618
#define CYD_DARK_GRAY   0x4208
#define CYD_SKY_BLUE    0x867D
#endif
