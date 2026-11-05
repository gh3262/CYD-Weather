#include "app.h"

#include <Fonts/FreeMono9pt7b.h>

#include "adafruitio/adafruitio_manager.h"
#include "screens/precipitation_page_values.h"

static const size_t PRECIPITATION_ROW_COUNT = 7;
static const size_t PRECIPITATION_VALUE_COUNT = 3;
static const int PRECIPITATION_FIRST_BASELINE = 47;
static const int PRECIPITATION_ROW_SPACING = 19;
static const int PRECIPITATION_COLUMN_X[4] = {8, 96, 184, 266};

struct PrecipitationRow
{
    const char *time;
};

static const PrecipitationRow PRECIPITATION_ROWS[PRECIPITATION_ROW_COUNT] = {
    {"1 hr"}, {"2 hr"}, {"6 hr"}, {"12 hr"},
    {"1 day"}, {"2 days"}, {"5 days"},
};

static const char *const PRECIPITATION_PAGE_FEED_KEYS[] = {
    "pages.p02-precipitation",
};

static String precipitationValues[PRECIPITATION_ROW_COUNT][PRECIPITATION_VALUE_COUNT];
static String renderedPrecipitationValues[PRECIPITATION_ROW_COUNT][PRECIPITATION_VALUE_COUNT];
static bool forcePrecipitationRender = true;
static String lastPrecipitationPagePayload;

static String formatPrecipitationPageValue(
    const PrecipitationPageValues &values, size_t row, size_t column)
{
    String value(values[row * PRECIPITATION_VALUE_COUNT + column].data());
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
    PrecipitationPageValues pageValues;
    bool hasPageValues = false;
    {
        String payload;
        if (adafruitIOManager.getFeedValue(
                PRECIPITATION_PAGE_FEED_KEYS[0], payload))
        {
            const char *error =
                parsePrecipitationPageValues(payload.c_str(), pageValues);
            hasPageValues = !error;
            if (error &&
                (forcePrecipitationRender ||
                 payload != lastPrecipitationPagePayload))
            {
                Serial.printf(
                    "Precip page feed: invalid pages.p02-precipitation: %s.\n",
                    error);
            }
            lastPrecipitationPagePayload = payload;
        }
    }

    for (size_t row = 0; row < PRECIPITATION_ROW_COUNT; ++row)
    {
        for (size_t column = 0; column < PRECIPITATION_VALUE_COUNT; ++column)
        {
            precipitationValues[row][column] = hasPageValues
                ? formatPrecipitationPageValue(pageValues, row, column)
                : String("--");
        }
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
        drawNav3("History");
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
            PRECIPITATION_PAGE_FEED_KEYS, 1))
    {
        Serial.println("Precipitation: could not activate pages.p02-precipitation.");
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
    else if (navPressed3(x, y))
        enterPrecipitationHistoryScreen();
}

void refreshPrecipitationResults()
{
    updatePrecipitationTable(false);
}
