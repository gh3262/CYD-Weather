#ifndef WIND_SNAPSHOT_TEST_APP_H
#define WIND_SNAPSHOT_TEST_APP_H

#include <Arduino.h>
#include <WiFi.h>
#include <vector>

using std::max;
static const double PI = 3.14159265358979323846;
static const uint16_t CYD_WHITE = 0xffff;
static const uint16_t CYD_RED = 0xf800;
static const uint16_t CYD_YELLOW = 0xffe0;
static const uint16_t CYD_CYAN = 0x07ff;
static const uint16_t CYD_GRAY = 0x8410;
static const uint16_t CYD_DARK_GRAY = 0x4208;
static const uint16_t CYD_LIGHT_GRAY = 0xc618;
static const uint16_t COLOR_TEXT = CYD_WHITE;
static const uint16_t COLOR_BACKGROUND = 0;
static const uint16_t COLOR_WARNING = CYD_YELLOW;
static const int FONT_MONO_SMALL = 1;
static const int SCREEN_WIDTH = 320;
static const int HEADER_HEIGHT = 28;
static const int NAV_Y = 210;
static const int SCREEN_WIND_DIRECTION = 1;
struct AppState { int currentScreen = 0; };
extern AppState state;

struct TestGraphics
{
    std::vector<std::string> printed;
    size_t redraws = 0;
    size_t petals = 0;
    size_t averages = 0;
    uint16_t arrowColor = 0;
    void setFont(int) {}
    void setTextSize(int) {}
    void setTextColor(uint16_t) {}
    void fillRect(int, int, int, int, uint16_t) { ++redraws; printed.clear(); }
    void setCursor(int, int) {}
    int getCursorX() { return 0; }
    int getCursorY() { return 0; }
    void print(const char *text) { printed.emplace_back(text); }
    void print(const String &text) { printed.emplace_back(text.c_str()); }
    void print(char text) { printed.emplace_back(1, text); }
    void print(int number) { printed.emplace_back(std::to_string(number)); }
    void drawCircle(int, int, int, uint16_t) {}
    void drawLine(int, int, int, int, uint16_t) {}
    void fillArc(int, int, int, int, double, double, uint16_t color)
    {
        if (color == 0x2AD4)
            ++petals;
    }
    void fillTriangle(int, int, int, int, int, int, uint16_t color)
    {
        if (color == CYD_RED)
            ++averages;
    }
    void fillCircle(int, int, int, uint16_t color) { arrowColor = color; }
};
extern TestGraphics *gfx;
void drawCurrentScreen();
void clearScreen(uint16_t);
void titleBar(const char *);
void setDefaultFont();
void drawNav1(const char *);
void drawNav3(const char *);
void drawNav4(const char *);
bool navPressed1(int, int);
bool navPressed3(int, int);
void enterWindScreen();
void enterWindHistoryScreen();
void drawWindDirectionScreen();
void refreshWindDirectionResults();
void leaveWindDirectionScreen();
void handleWindDirectionTouch(int, int);

#endif
