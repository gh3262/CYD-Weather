#include "app.h"

#include <Fonts/FreeMono9pt7b.h>

#include "adafruitio/adafruitio_manager.h"

static const int WIND_FIRST_BASELINE = 47;
static const int WIND_ROW_SPACING = 19;
static const int WIND_COLUMN_X[4] = {8, 110, 180, 257};
static const size_t WIND_VALUE_COUNT = 3;

struct WindRow
{
    const char *time;
    const char *feeds[WIND_VALUE_COUNT];
};

static const WindRow WIND_ROWS[] = {
    {"Current", {"currentwinddirection", "currentwindspeed", "currentwindgust"}},
    {"1 Hour", {"hours1winddirection", "hours1windspeed", "hours1windgust"}},
    {"2 Hours", {"hours2winddirection", "hours2windspeed", "hours2windgust"}},
    {"6 Hours", {"hours6winddirection", "hours6windspeed", "hours6windgust"}},
    {"12 Hours", {"hours12winddirection", "hours12windspeed", "hours12windgust"}},
    {"24 Hours", {"hours24winddirection", "hours24windspeed", "hours24windgust"}},
};

static const size_t WIND_ROW_COUNT = sizeof(WIND_ROWS) / sizeof(WIND_ROWS[0]);
static String renderedWindValues[WIND_ROW_COUNT][WIND_VALUE_COUNT];
static bool forceWindRender = true;

static String readWindValue(const char *feedKey)
{
    String value;
    if (!adafruitIOManager.getFeedValue(feedKey, value))
        return "--";

    value.trim();
    return value.isEmpty() ? String("--") : value;
}

void refreshWindResults()
{
    gfx->setFont(FONT_MONO_SMALL);
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_TEXT);

    for (size_t row = 0; row < WIND_ROW_COUNT; ++row)
    {
        String values[WIND_VALUE_COUNT];
        bool changed = forceWindRender;
        for (size_t column = 0; column < WIND_VALUE_COUNT; ++column)
        {
            values[column] = readWindValue(WIND_ROWS[row].feeds[column]);
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
    const char *feeds[WIND_ROW_COUNT * WIND_VALUE_COUNT];
    for (size_t row = 0; row < WIND_ROW_COUNT; ++row)
    {
        for (size_t column = 0; column < WIND_VALUE_COUNT; ++column)
            feeds[row * WIND_VALUE_COUNT + column] = WIND_ROWS[row].feeds[column];
    }
    if (!adafruitIOManager.setActiveFeeds(feeds, WIND_ROW_COUNT * WIND_VALUE_COUNT))
        Serial.println("Wind: could not activate its Adafruit IO feeds.");

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
