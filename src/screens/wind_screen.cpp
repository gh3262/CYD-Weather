#include "app.h"

#include <Fonts/FreeMono9pt7b.h>

#include "adafruitio/adafruitio_manager.h"
#include "screens/weather_page_values.h"

static const int WIND_FIRST_BASELINE = 47;
static const int WIND_ROW_SPACING = 19;
static const int WIND_COLUMN_X[4] = {8, 110, 180, 257};
static const size_t WIND_VALUE_COUNT = 3;

struct WindRow
{
    const char *time;
};

static const WindRow WIND_ROWS[] = {
    {"Current"}, {"1 Hour"}, {"2 Hours"}, {"6 Hours"}, {"12 Hours"}, {"24 Hours"},
};

static const size_t WIND_ROW_COUNT = sizeof(WIND_ROWS) / sizeof(WIND_ROWS[0]);
static String renderedWindValues[WIND_ROW_COUNT][WIND_VALUE_COUNT];
static bool forceWindRender = true;
static const char *const WIND_FEEDS[] = {"pages.p03-wind"};
static String lastWindPayload;

void refreshWindResults()
{
    WindPageValues pageValues;
    String payload;
    const bool available = adafruitIOManager.getFeedValue(WIND_FEEDS[0], payload);
    const char *error = available ? parseWindPageValues(payload.c_str(), pageValues) : nullptr;
    if (error && (forceWindRender || payload != lastWindPayload))
        Serial.printf("Wind: invalid %s: %s.\n", WIND_FEEDS[0], error);
    lastWindPayload = payload;
    gfx->setFont(FONT_MONO_SMALL);
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_TEXT);

    for (size_t row = 0; row < WIND_ROW_COUNT; ++row)
    {
        String values[WIND_VALUE_COUNT];
        bool changed = forceWindRender;
        for (size_t column = 0; column < WIND_VALUE_COUNT; ++column)
        {
            values[column] = available && !error
                ? String(pageValues[row * WIND_VALUE_COUNT + column].data()) : String("--");
            if (values[column].isEmpty())
                values[column] = "--";
            changed = changed || values[column] != renderedWindValues[row][column];
        }
        if (!changed)
            continue;

        const int baseline = WIND_FIRST_BASELINE +
            static_cast<int>(row + 1) * WIND_ROW_SPACING;
        gfx->fillRect(0, baseline - 14, SCREEN_WIDTH, WIND_ROW_SPACING,
                      COLOR_BACKGROUND);
        gfx->setCursor(WIND_COLUMN_X[0], baseline);
        gfx->print(WIND_ROWS[row].time);
        for (size_t column = 0; column < WIND_VALUE_COUNT; ++column)
        {
            gfx->setCursor(WIND_COLUMN_X[column + 1], baseline);
            gfx->print(values[column]);
            renderedWindValues[row][column] = values[column];
        }
    }

    forceWindRender = false;
    setDefaultFont();
    gfx->setTextSize(2);
}

void drawWindScreen()
{
    if (!adafruitIOManager.setActiveFeeds(WIND_FEEDS, 1))
        Serial.println("Wind: could not activate pages.p03-wind.");

    clearScreen(COLOR_BACKGROUND);
    titleBar("Wind");
    gfx->setFont(FONT_MONO_SMALL);
    gfx->setTextSize(1);
    gfx->setTextColor(CYD_CYAN);
    const char *const headers[] = {"Time", "Dir", "Speed", "Gust"};
    for (size_t column = 0; column < 4; ++column)
    {
        gfx->setCursor(WIND_COLUMN_X[column], WIND_FIRST_BASELINE);
        gfx->print(headers[column]);
    }

    forceWindRender = true;
    refreshWindResults();
    drawNav1("Back");
    drawNav2("Dir");
    drawNav3("Speed");
    drawNav4("Home");
}

void enterWindScreen()
{
    state.currentScreen = SCREEN_WIND;
    drawCurrentScreen();
}

void handleWindTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterDetailsScreen();
    else if (navPressed2(x, y))
        enterWindDirectionScreen();
    else if (navPressed3(x, y))
        enterWindHistoryScreen();
}
