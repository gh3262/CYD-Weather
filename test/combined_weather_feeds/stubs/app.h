#ifndef COMBINED_WEATHER_TEST_APP_H
#define COMBINED_WEATHER_TEST_APP_H

#include <Arduino.h>
#include <WiFi.h>
#include <cstdio>
#include <vector>

using std::max;

enum Screen
{
    SCREEN_DETAILS,
    SCREEN_TEMPERATURES,
    SCREEN_TEMPERATURE_HISTORY,
    SCREEN_TEMPERATURE_DAILY,
    SCREEN_PRECIPITATION,
    SCREEN_PRECIPITATION_HISTORY,
    SCREEN_WIND,
    SCREEN_WIND_HISTORY,
    SCREEN_WIND_DIRECTION,
    SCREEN_CLOUDS,
    SCREEN_BAROMETER,
    SCREEN_BAROMETER_HISTORY,
    SCREEN_BAROMETER_TREND,
    SCREEN_ASTRONOMICAL
};
struct AppState { Screen currentScreen; };
extern AppState state;

static const uint16_t CYD_CYAN = 0x07ff;
static const uint16_t CYD_GREEN = 0x07e0;
static const uint16_t CYD_YELLOW = 0xffe0;
static const uint16_t CYD_NAVY = 0x000f;
static const uint16_t COLOR_TEXT = 0xffff;
static const uint16_t COLOR_BACKGROUND = 0;
static const uint16_t COLOR_WARNING = 0xffe0;
static const uint16_t CYD_DARK_GRAY = 0x4208;
static const uint16_t CYD_GRAY = 0x8410;
static const int FONT_MONO_SMALL = 1;
static const int SCREEN_WIDTH = 320;
static const int HEADER_HEIGHT = 28;
static const int NAV_Y = 210;

struct TestGraphics
{
    std::vector<std::string> printed;
    void setFont(int) {}
    void setTextSize(int) {}
    void setTextColor(uint16_t) {}
    void fillRect(int, int, int, int, uint16_t) {}
    void setCursor(int, int) {}
    void print(const char *text) { printed.emplace_back(text); }
    void print(const String &text) { printed.emplace_back(text.c_str()); }
    void print(char text) { printed.emplace_back(1, text); }
};
extern TestGraphics *gfx;

inline String formatTemperatureValue(float value, unsigned decimals)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f F (%.1f C)", decimals,
                  static_cast<double>(value), (value - 32.0) / 1.8);
    return String(buffer);
}
void drawCurrentScreen();
void clearScreen(uint16_t);
void titleBar(const char *);
void setDefaultFont();
void centerText(const char *, int, uint16_t, int);
void drawNav1(const char *);
void drawNav2(const char *);
void drawNav3(const char *);
void drawNav4(const char *);
bool navPressed1(int, int);
bool navPressed2(int, int);
bool navPressed3(int, int);
void enterDetailsScreen();
void enterTemperatureDailyScreen();
void drawTemperatureDailyScreen();
void refreshTemperatureDailyResults();
void handleTemperatureDailyTouch(int, int);
void enterWindDirectionScreen();
void enterCloudsScreen();
void drawCloudsScreen();
void refreshCloudsResults();
void handleCloudsTouch(int, int);

void enterTemperaturesScreen();
void drawTemperaturesScreen();
void refreshTemperaturesResults();
void handleTemperaturesTouch(int, int);
void enterTemperatureHistoryScreen();
void drawTemperatureHistoryScreen();
void refreshTemperatureHistoryResults();
void handleTemperatureHistoryTouch(int, int);
void enterPrecipitationScreen();
void drawPrecipitationScreen();
void refreshPrecipitationResults();
void handlePrecipitationTouch(int, int);
void enterPrecipitationHistoryScreen();
void drawPrecipitationHistoryScreen();
void refreshPrecipitationHistoryResults();
void handlePrecipitationHistoryTouch(int, int);
void enterWindScreen();
void drawWindScreen();
void refreshWindResults();
void handleWindTouch(int, int);
void enterWindHistoryScreen();
void drawWindHistoryScreen();
void refreshWindHistoryResults();
void handleWindHistoryTouch(int, int);
void enterBarometerScreen();
void drawBarometerScreen();
void refreshBarometerResults();
void handleBarometerTouch(int, int);
void enterBarometerHistoryScreen();
void drawBarometerHistoryScreen();
void refreshBarometerHistoryResults();
void handleBarometerHistoryTouch(int, int);
void enterBarometerTrendScreen();
void drawBarometerTrendScreen();
void refreshBarometerTrendResults();
void handleBarometerTrendTouch(int, int);
void enterAstronomicalScreen();
void drawAstronomicalScreen();
void refreshAstronomicalResults();
void handleAstronomicalTouch(int, int);

#endif
