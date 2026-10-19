#include "app.h"

#include <Fonts/FreeMono9pt7b.h>

#include "adafruitio/adafruitio_manager.h"

static const size_t PRECIPITATION_ROW_COUNT = 7;
static const size_t PRECIPITATION_VALUE_COUNT = 3;
static const int PRECIPITATION_FIRST_BASELINE = 47;
static const int PRECIPITATION_ROW_SPACING = 19;
static const int PRECIPITATION_COLUMN_X[4] = {8, 96, 184, 266};

struct PrecipitationRow
{
    const char *time;
    const char *chanceFeed;
    const char *typeFeed;
    const char *amountFeed;
};

static const PrecipitationRow PRECIPITATION_ROWS[PRECIPITATION_ROW_COUNT] = {
    {"1 hr", "hours1precipitationchance", "hours1precipitationtype", "hours1precipitationamount"},
    {"2 hr", "hours2precipitationchance", "hours2precipitationtype", "hours2precipitationamount"},
    {"6 hr", "hours6precipitationchance", "hours6precipitationtype", "hours6precipitationamount"},
    {"12 hr", "hours12precipitationchance", "hours12precipitationtype", "hours12precipitationamount"},
    {"1 day", "days1precipitationchance", "days1precipitationtype", "days1precipitationamount"},
    {"2 days", "days2precipitationchance", "days2precipitationtype", "days2precipitationamount"},
    {"5 days", "days5precipitationchance", "days5precipitationtype", "days5precipitationamount"},
};

static const char *const PRECIPITATION_FEED_KEYS[] = {
    "hours1precipitationchance", "hours1precipitationtype", "hours1precipitationamount",
    "hours2precipitationchance", "hours2precipitationtype", "hours2precipitationamount",
    "hours6precipitationchance", "hours6precipitationtype", "hours6precipitationamount",
    "hours12precipitationchance", "hours12precipitationtype", "hours12precipitationamount",
    "days1precipitationchance", "days1precipitationtype", "days1precipitationamount",
    "days2precipitationchance", "days2precipitationtype", "days2precipitationamount",
    "days5precipitationchance", "days5precipitationtype", "days5precipitationamount",
};

static String precipitationValues[PRECIPITATION_ROW_COUNT][PRECIPITATION_VALUE_COUNT];
static String renderedPrecipitationValues[PRECIPITATION_ROW_COUNT][PRECIPITATION_VALUE_COUNT];
static bool forcePrecipitationRender = true;

static String readPrecipitationValue(const char *feedKey, size_t column)
{
    String value;
    if (!adafruitIOManager.getFeedValue(feedKey, value))
        return "--";

    value.trim();
    if (value.isEmpty())
        return "--";

    if (column == 0 && !value.endsWith("%"))
        value += "%";

    if (column == 1 && value.length() > 6)
        value = value.substring(0, 6);

    return value;
}

static void drawPrecipitationTable()
{
    gfx->fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, NAV_Y - HEADER_HEIGHT,
                  COLOR_BACKGROUND);
    gfx->setFont(FONT_MONO_SMALL);
    gfx->setTextSize(1);

    const char *const headers[4] = {"Time", "Chance", "Type", "Amt"};
    gfx->setTextColor(CYD_CYAN);
    for (size_t column = 0; column < 4; ++column)
    {
        gfx->setCursor(PRECIPITATION_COLUMN_X[column],
                       PRECIPITATION_FIRST_BASELINE);
        gfx->print(headers[column]);
    }

    gfx->setTextColor(COLOR_TEXT);
    for (size_t row = 0; row < PRECIPITATION_ROW_COUNT; ++row)
    {
        const int baseline = PRECIPITATION_FIRST_BASELINE +
            static_cast<int>(row + 1) * PRECIPITATION_ROW_SPACING;
        gfx->setCursor(PRECIPITATION_COLUMN_X[0], baseline);
        gfx->print(PRECIPITATION_ROWS[row].time);
        for (size_t column = 0; column < PRECIPITATION_VALUE_COUNT; ++column)
        {
            gfx->setCursor(PRECIPITATION_COLUMN_X[column + 1], baseline);
            gfx->print(precipitationValues[row][column]);
        }
    }

    setDefaultFont();
    gfx->setTextSize(2);
}

static void updatePrecipitationTable(bool redrawPage)
{
    bool changed = forcePrecipitationRender;
    for (size_t row = 0; row < PRECIPITATION_ROW_COUNT; ++row)
    {
        precipitationValues[row][0] =
            readPrecipitationValue(PRECIPITATION_ROWS[row].chanceFeed, 0);
        precipitationValues[row][1] =
            readPrecipitationValue(PRECIPITATION_ROWS[row].typeFeed, 1);
        precipitationValues[row][2] =
            readPrecipitationValue(PRECIPITATION_ROWS[row].amountFeed, 2);
        for (size_t column = 0; column < PRECIPITATION_VALUE_COUNT; ++column)
        {
            changed = changed ||
                precipitationValues[row][column] !=
                    renderedPrecipitationValues[row][column];
        }
    }

    if (!changed && !redrawPage)
        return;

    if (redrawPage)
    {
        clearScreen(COLOR_BACKGROUND);
        titleBar("Precipitation");
        drawNav1("Back");
        drawNav4("Home");
    }
    drawPrecipitationTable();

    for (size_t row = 0; row < PRECIPITATION_ROW_COUNT; ++row)
    {
        for (size_t column = 0; column < PRECIPITATION_VALUE_COUNT; ++column)
            renderedPrecipitationValues[row][column] =
                precipitationValues[row][column];
    }
    forcePrecipitationRender = false;
}

void drawPrecipitationScreen()
{
    if (!adafruitIOManager.setActiveFeeds(
            PRECIPITATION_FEED_KEYS,
            sizeof(PRECIPITATION_FEED_KEYS) / sizeof(PRECIPITATION_FEED_KEYS[0])))
    {
        Serial.println("Precipitation: could not activate its Adafruit IO feeds.");
    }

    forcePrecipitationRender = true;
    updatePrecipitationTable(true);
}

void enterPrecipitationScreen()
{
    state.currentScreen = SCREEN_PRECIPITATION;
    drawCurrentScreen();
}

void handlePrecipitationTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterDetailsScreen();
}

void refreshPrecipitationResults()
{
    updatePrecipitationTable(false);
}
