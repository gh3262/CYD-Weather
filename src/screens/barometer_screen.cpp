#include "app.h"

#include <Fonts/FreeMono9pt7b.h>

#include "adafruitio/adafruitio_manager.h"
#include "screens/weather_page_values.h"

static const int BAROMETER_FIRST_BASELINE = 47;
static const int BAROMETER_ROW_SPACING = 19;
static const int BAROMETER_COLUMN_X[3] = {8, 110, 210};

struct BarometerRow
{
    const char *time;
};

static const BarometerRow BAROMETER_ROWS[] = {
    {"Current"}, {"1 Hour"}, {"2 Hours"}, {"6 Hours"}, {"12 Hours"}, {"24 Hours"},
};

static const size_t BAROMETER_ROW_COUNT =
    sizeof(BAROMETER_ROWS) / sizeof(BAROMETER_ROWS[0]);
static String renderedBarometerValues[BAROMETER_ROW_COUNT][2];
static bool forceBarometerRender = true;
static const char *const BAROMETER_FEEDS[] = {"pages.p04-baro"};
static String lastBarometerPayload;

void refreshBarometerResults()
{
    BarometerPageValues pageValues;
    String payload;
    const bool available = adafruitIOManager.getFeedValue(BAROMETER_FEEDS[0], payload);
    const char *error = available
        ? parseBarometerPageValues(payload.c_str(), pageValues) : nullptr;
    if (error && (forceBarometerRender || payload != lastBarometerPayload))
        Serial.printf("Barometer: invalid %s: %s.\n", BAROMETER_FEEDS[0], error);
    lastBarometerPayload = payload;
    gfx->setFont(FONT_MONO_SMALL);
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_TEXT);

    for (size_t row = 0; row < BAROMETER_ROW_COUNT; ++row)
    {
        const String pressure = available && !error
            ? String(pageValues[row * 2].data()) : String("--");
        String trend = available && !error
            ? String(pageValues[row * 2 + 1].data()) : String("--");
        if (trend.isEmpty())
            trend = "--";
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
    if (!adafruitIOManager.setActiveFeeds(BAROMETER_FEEDS, 1))
        Serial.println("Barometer: could not activate pages.p04-baro.");

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
    drawNav2("Trend");
    drawNav3("History");
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
    else if (navPressed2(x, y))
        enterBarometerTrendScreen();
    else if (navPressed3(x, y))
        enterBarometerHistoryScreen();
}
