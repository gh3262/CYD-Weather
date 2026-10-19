#include "app.h"

#include "adafruitio/adafruitio_manager.h"
#include "screens/text_table_page.h"

static const char *const TEMPERATURE_FEED_KEYS[] = {
    "feed-avg",
    "wapi.feels-like",
    "currenttemperatureapparent",
    "currenttemperaturedewpoint",
    "wapi.heat-index",
    "wapi.wind-chill",
};

static const size_t TEMPERATURE_ROW_COUNT =
    sizeof(TEMPERATURE_FEED_KEYS) / sizeof(TEMPERATURE_FEED_KEYS[0]);

static const char *const TEMPERATURE_LABELS[TEMPERATURE_ROW_COUNT] = {
    "Temperature",
    "Feels Like",
    "Apparent",
    "Dew Point",
    "Heat Index",
    "Wind Chill",
};

static const TextTableColor TEMPERATURE_LABEL_COLORS[TEMPERATURE_ROW_COUNT] = {
    TextTableColor::Cyan,
    TextTableColor::Yellow,
    TextTableColor::Yellow,
    TextTableColor::Cyan,
    TextTableColor::Pink,
    TextTableColor::Pink,
};

static String temperatureValues[TEMPERATURE_ROW_COUNT];
static String renderedTemperatureValues[TEMPERATURE_ROW_COUNT];
static bool forceTemperatureRender = true;

static String readTemperatureValue(const char *feedKey)
{
    String value;
    if (!adafruitIOManager.getFeedValue(feedKey, value))
        return "-- F";

    return formatTemperatureFeedValue(value, 1);
}

static TextTablePage temperaturesPage(const TextTableRow *rows)
{
    return {
        "Temperatures",
        CYD_NAVY,
        130,
        rows,
        rows ? TEMPERATURE_ROW_COUNT : 0,
        enterDetailsScreen,
        {nullptr, nullptr},
        {nullptr, nullptr},
    };
}

static void updateTemperatureRows(bool redrawPage)
{
    bool changed = forceTemperatureRender;
    for (size_t i = 0; i < TEMPERATURE_ROW_COUNT; ++i)
    {
        temperatureValues[i] = readTemperatureValue(TEMPERATURE_FEED_KEYS[i]);
        changed = changed || temperatureValues[i] != renderedTemperatureValues[i];
    }
    if (!changed)
        return;

    TextTableRow rows[TEMPERATURE_ROW_COUNT];
    for (size_t i = 0; i < TEMPERATURE_ROW_COUNT; ++i)
    {
        rows[i] = {
            TEMPERATURE_LABELS[i],
            temperatureValues[i].c_str(),
            TEMPERATURE_LABEL_COLORS[i],
            TextTableColor::White,
        };
    }

    const TextTablePage page = temperaturesPage(rows);
    const bool rendered = redrawPage
        ? drawTextTablePage(page)
        : refreshTextTablePageRows(page);
    if (rendered)
    {
        for (size_t i = 0; i < TEMPERATURE_ROW_COUNT; ++i)
            renderedTemperatureValues[i] = temperatureValues[i];
        forceTemperatureRender = false;
    }
}

void drawTemperaturesScreen()
{
    if (!adafruitIOManager.setActiveFeeds(TEMPERATURE_FEED_KEYS, TEMPERATURE_ROW_COUNT))
    {
        Serial.println("Temperatures: could not activate its Adafruit IO feeds.");
    }
    forceTemperatureRender = true;
    updateTemperatureRows(true);
}

void enterTemperaturesScreen()
{
    state.currentScreen = SCREEN_TEMPERATURES;
    drawCurrentScreen();
}

void handleTemperaturesTouch(int x, int y)
{
    handleTextTablePageTouch(temperaturesPage(nullptr), x, y);
}

void refreshTemperaturesResults()
{
    updateTemperatureRows(false);
}
