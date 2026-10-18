#ifndef CYD_DISPLAY_H
#define CYD_DISPLAY_H
#include <Arduino_GFX_Library.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "cyd_pins.h"
#include "cyd_helpers.h"
#include "cyd_theme.h"
#include "cyd_time.h"
// ====================================================
// Display Objects
// ====================================================
extern Arduino_GFX *gfx;
// ====================================================
// Initialization
// ====================================================
inline bool initDisplay()
{
    backlightInit();
    if (!gfx->begin())
        return false;

    gfx->invertDisplay(true);

    return true;
}
// ====================================================
// Convenience Functions
// ====================================================
inline void clearScreen(uint16_t color = COLOR_BACKGROUND)
{
    gfx->fillScreen(color);
}

inline void setBodyFont()
{
    gfx->setFont(FONT_SANS_SMALL);
}

inline void setHeaderFont()
{
    gfx->setFont(FONT_SANS_NORMAL);
}

inline void setDefaultFont()
{
    gfx->setFont();
}

inline void drawClock()
{
    setDefaultFont();
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_HEADER_TEXT);

    // Clear only the clock area
    gfx->fillRect(
        245,
        5,
        70,
        25,
        COLOR_HEADER_BG);

    gfx->setCursor(248, 5);
    gfx->print(currentDateString());
    gfx->setCursor(248, 15);
    gfx->print(currentTimeString());
}
// Set from the Adafruit IO 'nbr-alerts' feed; shown on every title bar.
extern bool headerAlertActive;

static const int ALERT_INDICATOR_X = 234;
static const int ALERT_INDICATOR_Y = 14;
static const int ALERT_INDICATOR_RADIUS = 8;

// Inactive draws in the header color so a cleared alert disappears.
inline void drawAlertIndicator()
{
    gfx->fillCircle(
        ALERT_INDICATOR_X,
        ALERT_INDICATOR_Y,
        ALERT_INDICATOR_RADIUS,
        headerAlertActive ? CYD_BRIGHTRED : COLOR_HEADER_BG);
}

inline void titleBar(
    const char *title,
    uint16_t bgColor = COLOR_HEADER_BG,
    uint16_t textColor = COLOR_HEADER_TEXT)
{
    gfx->fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, bgColor);
    setHeaderFont();
    gfx->setTextColor(textColor);
    gfx->setTextSize(1);
    gfx->setCursor(5, 22);
    gfx->print(title);

    setDefaultFont();
    drawClock();

    // The header was just filled, so only an active alert needs drawing.
    if (headerAlertActive)
        drawAlertIndicator();
}

inline void centerText(
    const char *text,
    int y,
    uint16_t color = COLOR_TEXT,
    int textSize = 2)
{
    gfx->setTextSize(textSize);
    gfx->setTextColor(color);
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(
        text,
        0,
        0,
        &x1,
        &y1,
        &w,
        &h);
    int x = (SCREEN_WIDTH - w) / 2 - x1;
    gfx->setCursor(x, y - y1);
    gfx->print(text);
}

// ====================================================
// Simple Button
// ====================================================
inline void drawButton(
    int x,
    int y,
    int w,
    int h,
    const char *label,
    uint16_t buttonColor = COLOR_BUTTON,
    uint16_t textColor = COLOR_BUTTON_TEXT)
{
    gfx->fillRoundRect(
        x,
        y,
        w,
        h,
        8,
        buttonColor);
    setBodyFont();
    gfx->setTextColor(textColor);
    gfx->setTextSize(1);
    int16_t x1, y1;
    uint16_t tw, th;
    gfx->getTextBounds(
        label,
        0,
        0,
        &x1,
        &y1,
        &tw,
        &th);
    gfx->setCursor(
        x + (w - tw) / 2 - x1,
        y + (h - th) / 2 - y1);
    gfx->print(label);
    setDefaultFont();
    gfx->setTextSize(2);
}

// ====================================================
// Navigation Buttons
// ====================================================

inline void drawNav1(const char *label)
{
    drawButton(NAV_X1, NAV_Y, NAV_WIDTH, NAV_HEIGHT, label);
}

inline void drawNav2(const char *label)
{
    drawButton(NAV_X2, NAV_Y, NAV_WIDTH, NAV_HEIGHT, label);
}

inline void drawNav3(const char *label)
{
    drawButton(NAV_X3, NAV_Y, NAV_WIDTH, NAV_HEIGHT, label);
}

inline void drawNav4(const char *label)
{
    drawButton(NAV_X4, NAV_Y, NAV_WIDTH, NAV_HEIGHT, label);
}
#endif
