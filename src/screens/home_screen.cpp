#include "app.h"
#include "adafruitio/adafruitio_manager.h"

#include <math.h>

static const int HOME_TIME_TOP = HEADER_HEIGHT;
static const int HOME_TIME_BOTTOM = 108;
static const int HOME_TEMPERATURE_TOP = 108;
static const int HOME_TEMPERATURE_BOTTOM = 150;

// Alert button sits in the open space between temperature and the nav bar.
static const int HOME_ALERT_AREA_TOP = HOME_TEMPERATURE_BOTTOM;
static const int HOME_ALERT_AREA_BOTTOM = NAV_Y - 2;
static const int HOME_ALERT_BUTTON_W = 200;
static const int HOME_ALERT_BUTTON_H = 36;
static const int HOME_ALERT_BUTTON_X = (SCREEN_WIDTH - HOME_ALERT_BUTTON_W) / 2;
static const int HOME_ALERT_BUTTON_Y =
    (HOME_ALERT_AREA_TOP + HOME_ALERT_AREA_BOTTOM - HOME_ALERT_BUTTON_H) / 2;

static String lastRenderedTemperature;
static long renderedAlertCount = -1;

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

static void refreshHomeAlertButton()
{
    const long count = readAlertCount();
    if (count == renderedAlertCount)
        return;
    renderedAlertCount = count;

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

    setDefaultFont();
    gfx->setTextSize(2);
}

void refreshHomeScreen()
{
    refreshHomeTemperature();
    refreshHomeAlertButton();
}

void drawHomeScreen()
{
    clearScreen(COLOR_BACKGROUND);
    lastRenderedTemperature = "";
    renderedAlertCount = -1;

    titleBar(
        "HOME",
        COLOR_HEADER_BG,
        COLOR_HEADER_TEXT);

    const char *activeFeeds[] = {"feed-avg"};
    adafruitIOManager.setActiveFeeds(activeFeeds, 1);

    refreshHomeTemperature();
    refreshHomeTime();
    refreshHomeAlertButton();

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
        touchInRect(x, y, HOME_ALERT_BUTTON_X, HOME_ALERT_BUTTON_Y,
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
