#include "app.h"
#include "adafruitio/adafruitio_manager.h"
#include "screens/weather_descriptor.h"

#include <Fonts/FreeSans18pt7b.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>

static const int HOME_TIME_TOP = HEADER_HEIGHT;
static const int HOME_TIME_BOTTOM = 108;
static const int HOME_TEMPERATURE_TOP = 108;
static const int HOME_TEMPERATURE_BOTTOM = 150;

// Alerts take precedence over the weather word in this shared area.
static const int HOME_ALERT_AREA_TOP = HOME_TEMPERATURE_BOTTOM;
static const int HOME_ALERT_AREA_BOTTOM = NAV_Y - 2;
static const int HOME_ALERT_BUTTON_W = 200;
static const int HOME_ALERT_BUTTON_H = 36;
static const int HOME_ALERT_BUTTON_X = (SCREEN_WIDTH - HOME_ALERT_BUTTON_W) / 2;
static const int HOME_ALERT_BUTTON_Y =
    (HOME_ALERT_AREA_TOP + HOME_ALERT_AREA_BOTTOM - HOME_ALERT_BUTTON_H) / 2;

static String lastRenderedTemperature;
static String lastRenderedWeather;
static long renderedAlertCount = -1;
static bool invalidPressureLogged = false;

void refreshHomeTime()
{
    const int centerY = (HOME_TIME_TOP + HOME_TIME_BOTTOM) / 2;

    gfx->fillRect(
        0,
        HOME_TIME_TOP,
        SCREEN_WIDTH,
        HOME_TIME_BOTTOM - HOME_TIME_TOP,
        COLOR_BACKGROUND);

    const char *time = currentTimeString();
    gfx->setFont(FONT_EXTRA_LARGE);
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_TEXT);

    int16_t x1, y1;
    uint16_t width, height;
    gfx->getTextBounds(time, 0, 0, &x1, &y1, &width, &height);
    gfx->setCursor(
        (SCREEN_WIDTH - width) / 2 - x1,
        centerY - (height / 2) - y1);
    gfx->print(time);

    setDefaultFont();
    gfx->setTextSize(2);
}

void refreshHomeTemperature()
{
    String feedValue;
    String temperature = "-- F";
    if (adafruitIOManager.getFeedValue("feed-avg", feedValue))
    {
        temperature = String(static_cast<long>(lroundf(feedValue.toFloat())));
        temperature += " F";
    }

    if (temperature == lastRenderedTemperature)
        return;
    lastRenderedTemperature = temperature;

    gfx->fillRect(
        0,
        HOME_TEMPERATURE_TOP,
        SCREEN_WIDTH,
        HOME_TEMPERATURE_BOTTOM - HOME_TEMPERATURE_TOP,
        COLOR_BACKGROUND);

    gfx->setFont(FONT_EXTRA_LARGE);
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_TEXT);

    int16_t x1, y1;
    uint16_t width, height;
    gfx->getTextBounds(
        temperature.c_str(), 0, 0, &x1, &y1, &width, &height);
    const int centerY = (HOME_TEMPERATURE_TOP + HOME_TEMPERATURE_BOTTOM) / 2;
    gfx->setCursor(
        (SCREEN_WIDTH - width) / 2 - x1,
        centerY - (height / 2) - y1);
    gfx->print(temperature);

    setDefaultFont();
    gfx->setTextSize(2);
}

static long readAlertCount()
{
    String value;
    if (!adafruitIOManager.getFeedValue("nbr-alerts", value))
        return 0;
    const long count = value.toInt();
    return count > 0 ? count : 0;
}

static bool readHomePressure(const char *feedKey, float &pressure)
{
    String value;
    if (!adafruitIOManager.getFeedValue(feedKey, value))
        return false;

    char *end = nullptr;
    pressure = strtof(value.c_str(), &end);
    const bool parsed = end != value.c_str();
    while (*end && isspace(static_cast<unsigned char>(*end)))
        ++end;
    if (!parsed || *end || !validWeatherPressure(pressure))
    {
        if (!invalidPressureLogged)
            Serial.printf("Home: invalid pressure in feed '%s'; weather unavailable.\n", feedKey);
        invalidPressureLogged = true;
        return false;
    }
    return true;
}

static const char *readHomeWeather()
{
    float pressure;
    float forecastPressure;
    if (!readHomePressure("currentpressure", pressure) ||
        !readHomePressure("hours24pressure", forecastPressure))
        return "--";

    invalidPressureLogged = false;
    return weatherDescriptor(pressure, forecastPressure);
}

static void refreshHomeWeather()
{
    const long count = readAlertCount();
    const String weather = count > 0 ? "" : readHomeWeather();
    if (count == renderedAlertCount && weather == lastRenderedWeather)
        return;
    renderedAlertCount = count;
    lastRenderedWeather = weather;

    gfx->fillRect(
        0,
        HOME_ALERT_AREA_TOP,
        SCREEN_WIDTH,
        HOME_ALERT_AREA_BOTTOM - HOME_ALERT_AREA_TOP,
        COLOR_BACKGROUND);

    if (count > 0)
    {
        char label[24];
        snprintf(label, sizeof(label), "%ld Active Alert%s",
                 count, count == 1 ? "" : "s");
        drawButton(
            HOME_ALERT_BUTTON_X,
            HOME_ALERT_BUTTON_Y,
            HOME_ALERT_BUTTON_W,
            HOME_ALERT_BUTTON_H,
            label,
            CYD_RED,
            CYD_WHITE);
    }
    else
    {
        gfx->setFont(FONT_SANS_LARGE);
        gfx->setTextSize(1);
        gfx->setTextColor(CYD_CYAN);
        int16_t x1, y1;
        uint16_t width, height;
        gfx->getTextBounds(weather.c_str(), 0, 0, &x1, &y1, &width, &height);
        gfx->setCursor(
            (SCREEN_WIDTH - width) / 2 - x1,
            (HOME_ALERT_AREA_TOP + HOME_ALERT_AREA_BOTTOM - height) / 2 - y1);
        gfx->print(weather);
    }

    setDefaultFont();
    gfx->setTextSize(2);
}

void refreshHomeScreen()
{
    refreshHomeTemperature();
    refreshHomeWeather();
}

void drawHomeScreen()
{
    clearScreen(COLOR_BACKGROUND);
    lastRenderedTemperature = "";
    lastRenderedWeather = "";
    renderedAlertCount = -1;
    invalidPressureLogged = false;

    titleBar(
        "HOME",
        COLOR_HEADER_BG,
        COLOR_HEADER_TEXT);

    const char *activeFeeds[] = {"feed-avg", "currentpressure", "hours24pressure"};
    if (!adafruitIOManager.setActiveFeeds(
            activeFeeds, sizeof(activeFeeds) / sizeof(activeFeeds[0])))
        Serial.println("Home: could not activate its Adafruit IO feeds.");

    refreshHomeTemperature();
    refreshHomeTime();
    refreshHomeWeather();

    drawNav1("Now");
    drawNav2("Forecast");
    drawNav3("Details");
    drawNav4("System");
}

void enterHomeScreen()
{
    state.currentScreen = SCREEN_HOME;
    drawCurrentScreen();
}

void handleHomeTouch(int x, int y)
{
    if (renderedAlertCount > 0 &&
        buttonPressed(x, y, HOME_ALERT_BUTTON_X, HOME_ALERT_BUTTON_Y,
                    HOME_ALERT_BUTTON_W, HOME_ALERT_BUTTON_H))
    {
        enterAlertsScreen();
        return;
    }

    if (navPressed1(x, y))
    {
        enterSnapshotScreen();
        return;
    }

    if (navPressed2(x, y))
    {
        enterForecastScreen();
        return;
    }

    if (navPressed3(x, y))
    {
        enterDetailsScreen();
        return;
    }

    // System button
    if (navPressed4(x, y))
    {
        adafruitIOManager.setActiveFeeds(nullptr, 0);
        enterSystemScreen();
    }
}
