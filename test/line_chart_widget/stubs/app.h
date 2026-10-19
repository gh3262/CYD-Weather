#ifndef CHART_TEST_APP_H
#define CHART_TEST_APP_H

#include <Arduino.h>
#include <algorithm>
#include <vector>
#include <string>

using std::max;
using std::min;

static const int SCREEN_WIDTH = 320;
static const int SCREEN_HEIGHT = 240;
static const int HEADER_HEIGHT = 30;
static const int NAV_Y = 200;
static const uint16_t COLOR_BACKGROUND = 1;
static const uint16_t CYD_CYAN = 2;
static const uint16_t COLOR_TEXT = 3;
static const uint16_t CYD_DARK_GRAY = 4;

struct RecordedLine
{
    int x0, y0, x1, y1;
    uint16_t color;
};

struct RecordedPixel
{
    int x, y;
    uint16_t color;
};

struct RecordedRect
{
    int x, y, width, height;
    uint16_t color;
};

struct RecordedText
{
    int x, y;
    std::string value;
    uint16_t color;
};

struct RecordingGraphics
{
    std::vector<RecordedLine> lines;
    std::vector<RecordedPixel> pixels;
    std::vector<RecordedRect> rectangles;
    std::vector<RecordedText> texts;
    int cursorX = 0;
    int cursorY = 0;
    int textSize = 2;
    uint16_t textColor = COLOR_TEXT;

    void setFont() {}
    void setTextSize(int size) { textSize = size; }
    void setTextColor(uint16_t color) { textColor = color; }
    void setCursor(int x, int y) { cursorX = x; cursorY = y; }
    void print(const char *value)
    {
        texts.push_back({cursorX, cursorY, value, textColor});
    }

    void fillRect(int x, int y, int width, int height, uint16_t color)
    {
        rectangles.push_back({x, y, width, height, color});
    }
    void drawLine(int x0, int y0, int x1, int y1, uint16_t color)
    {
        lines.push_back({x0, y0, x1, y1, color});
    }
    void drawPixel(int x, int y, uint16_t color)
    {
        pixels.push_back({x, y, color});
    }
    void reset()
    {
        lines.clear();
        pixels.clear();
        rectangles.clear();
        texts.clear();
    }
};

extern RecordingGraphics *gfx;

inline void setDefaultFont() { gfx->setFont(); }

#endif
