#include "app.h"
#include "adafruitio/adafruitio_manager.h"

#include <math.h>

static const int HOME_TIME_TOP = HEADER_HEIGHT;
static const int HOME_TIME_BOTTOM = 112;
static const int HOME_TEMPERATURE_TOP = 112;
static const int HOME_TEMPERATURE_BOTTOM = NAV_Y;
static String lastRenderedTemperature;

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

void drawHomeScreen()
{
    clearScreen(COLOR_BACKGROUND);
    lastRenderedTemperature = "";

    titleBar(
        "HOME",
        COLOR_HEADER_BG,
        COLOR_HEADER_TEXT);

    const char *activeFeeds[] = {"feed-avg"};
    adafruitIOManager.setActiveFeeds(activeFeeds, 1);

    refreshHomeTemperature();
    refreshHomeTime();

    drawNav4("System");
}

void enterHomeScreen()
{
    state.currentScreen = SCREEN_HOME;
    drawCurrentScreen();
}

void handleHomeTouch(int x, int y)
{
    // System button
    if (navPressed4(x, y))
    {
        adafruitIOManager.setActiveFeeds(nullptr, 0);
        enterSystemScreen();
    }
}
