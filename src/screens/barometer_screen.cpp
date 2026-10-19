#include "app.h"

#include <Fonts/FreeMono9pt7b.h>

#include "adafruitio/adafruitio_manager.h"

static const int BAROMETER_FIRST_BASELINE = 47;
static const int BAROMETER_ROW_SPACING = 19;
static const int BAROMETER_COLUMN_X[3] = {8, 110, 210};

struct BarometerRow
{
    const char *time;
    const char *pressureFeed;
    const char *trendFeed;
};

static const BarometerRow BAROMETER_ROWS[] = {
    {"Current", "currentpressure", "currentpressuretrend"},
    {"1 Hour", "hours1pressure", "hours1pressuretrend"},
    {"2 Hours", "hours2pressure", "hours2pressuretrend"},
    {"6 Hours", "hours6pressure", "hours6pressuretrend"},
    {"12 Hours", "hours12pressure", "hours12pressuretrend"},
    {"24 Hours", "hours24pressure", "hours24pressuretrend"},
};

static const size_t BAROMETER_ROW_COUNT =
    sizeof(BAROMETER_ROWS) / sizeof(BAROMETER_ROWS[0]);
static String renderedBarometerValues[BAROMETER_ROW_COUNT][2];
static bool forceBarometerRender = true;

static String readBarometerValue(const char *feedKey)
{
    String value;
    if (!adafruitIOManager.getFeedValue(feedKey, value))
        return "--";

    value.trim();
    return value.isEmpty() ? String("--") : value;
}

void refreshBarometerResults()
{
    gfx->setFont(FONT_MONO_SMALL);
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_TEXT);

    for (size_t row = 0; row < BAROMETER_ROW_COUNT; ++row)
    {
        const String pressure = readBarometerValue(BAROMETER_ROWS[row].pressureFeed);
        const String trend = readBarometerValue(BAROMETER_ROWS[row].trendFeed);
        if (!forceBarometerRender &&
            pressure == renderedBarometerValues[row][0] &&
            trend == renderedBarometerValues[row][1])
            continue;

        const int baseline = BAROMETER_FIRST_BASELINE +
            static_cast<int>(row + 1) * BAROMETER_ROW_SPACING;
        gfx->fillRect(0, baseline - 14, SCREEN_WIDTH, BAROMETER_ROW_SPACING,
                      COLOR_BACKGROUND);
        gfx->setCursor(BAROMETER_COLUMN_X[0], baseline);
        gfx->print(BAROMETER_ROWS[row].time);
        gfx->setCursor(BAROMETER_COLUMN_X[1], baseline);
        gfx->print(pressure);
        gfx->setCursor(BAROMETER_COLUMN_X[2], baseline);
        gfx->print(trend);

        renderedBarometerValues[row][0] = pressure;
        renderedBarometerValues[row][1] = trend;
    }

    forceBarometerRender = false;
    setDefaultFont();
    gfx->setTextSize(2);
}

void drawBarometerScreen()
{
    const char *feeds[BAROMETER_ROW_COUNT * 2];
    for (size_t row = 0; row < BAROMETER_ROW_COUNT; ++row)
    {
        feeds[row * 2] = BAROMETER_ROWS[row].pressureFeed;
        feeds[row * 2 + 1] = BAROMETER_ROWS[row].trendFeed;
    }
    if (!adafruitIOManager.setActiveFeeds(feeds, BAROMETER_ROW_COUNT * 2))
        Serial.println("Barometer: could not activate its Adafruit IO feeds.");

    clearScreen(COLOR_BACKGROUND);
    titleBar("Barometer");
    gfx->setFont(FONT_MONO_SMALL);
    gfx->setTextSize(1);
    gfx->setTextColor(CYD_CYAN);
    const char *const headers[] = {"Time", "Pressure", "Trend"};
    for (size_t column = 0; column < 3; ++column)
    {
        gfx->setCursor(BAROMETER_COLUMN_X[column], BAROMETER_FIRST_BASELINE);
        gfx->print(headers[column]);
    }

    forceBarometerRender = true;
    refreshBarometerResults();
    drawNav1("Back");
    drawNav4("Home");
}

void enterBarometerScreen()
{
    state.currentScreen = SCREEN_BAROMETER;
    drawCurrentScreen();
}

void handleBarometerTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterDetailsScreen();
}
