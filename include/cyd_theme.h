#ifndef CYD_THEME_H
#define CYD_THEME_H

#include "cyd_pins.h"

// ====================================================
// Colors
// ====================================================

#define COLOR_BACKGROUND   CYD_NAVY
#define COLOR_TEXT         CYD_WHITE
#define COLOR_LABEL_TEXT   CYD_CYAN
#define COLOR_HEADER_BG    CYD_BLACK
#define COLOR_HEADER_TEXT  CYD_WHITE

#define COLOR_OK           CYD_GREEN
#define COLOR_WARNING      CYD_YELLOW
#define COLOR_ERROR        CYD_RED

#define COLOR_BUTTON       CYD_DARK_GREEN
#define COLOR_BUTTON_ALT   CYD_DARK_RED
#define COLOR_BUTTON_OPT   CYD_DARK_GRAY
#define COLOR_BUTTON_TEXT  CYD_WHITE

// ====================================================
// Font Settings
// ====================================================

#define FONT_SMALL   1
#define FONT_NORMAL  2
#define FONT_LARGE   3
#define FONT_SANS_SMALL (&FreeSans9pt7b)
#define FONT_SANS_NORMAL (&FreeSans12pt7b)
#define FONT_SANS_LARGE (&FreeSans18pt7b)
#define FONT_MONO_SMALL (&FreeMono9pt7b)
#define FONT_MONO_NORMAL (&FreeMono12pt7b)
#define FONT_EXTRA_LARGE (&FreeSansBold24pt7b)

// ====================================================
// Screen Layout
// ====================================================

#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  240

#define HEADER_HEIGHT   30

// Touch handling
#define TOUCH_MIN_PRESSURE   200
#define TOUCH_RELEASE_MS      90   // finger must be up this long before a new press counts

// ====================================================
// Buttons
// ====================================================

// #define NAV_Y           200

// #define NAV_LEFT_X       10
// #define NAV_CENTER_X    115
// #define NAV_RIGHT_X     220

// #define NAV_WIDTH        90
// #define NAV_HEIGHT       30

#define NAV_BUTTONS 4

#define NAV_Y           200
#define NAV_X1  2
#define NAV_X2  81
#define NAV_X3 160
#define NAV_X4 239

#define NAV_WIDTH   78
#define NAV_HEIGHT  30



#endif