#ifndef TEMPERATURE_HISTORY_TEST_APP_H
#define TEMPERATURE_HISTORY_TEST_APP_H

#include <Arduino.h>
#include <WiFi.h>

enum Screen
{
    SCREEN_TEMPERATURES,
    SCREEN_TEMPERATURE_HISTORY,
    SCREEN_TEMPERATURE_DAILY,
    SCREEN_TEMPERATURE_HOURLY
};

struct AppState
{
    Screen currentScreen;
};

extern AppState state;
static const uint16_t CYD_CYAN = 0x07ff;

void drawCurrentScreen();
void drawNav2(const char *);
void drawNav3(const char *);
bool navPressed1(int x, int y);
bool navPressed2(int x, int y);
bool navPressed3(int x, int y);
void enterTemperaturesScreen();
void enterTemperatureDailyScreen();
void enterTemperatureHourlyScreen();
void enterTemperatureHistoryScreen();
void drawTemperatureHistoryScreen();
void refreshTemperatureHistoryResults();
void handleTemperatureHistoryTouch(int x, int y);

#endif
