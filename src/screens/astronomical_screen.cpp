#include "app.h"

#include "adafruitio/adafruitio_manager.h"
#include "screens/text_table_page.h"
#include "screens/weather_page_values.h"

static const char *const ASTRONOMICAL_FEED_KEYS[] = {
    "pages.p05-astro",
};

static const size_t ASTRONOMICAL_ROW_COUNT = 5;
static const size_t ASTRONOMICAL_DISPLAY_ROW_COUNT = ASTRONOMICAL_ROW_COUNT + 3;

static const char *const ASTRONOMICAL_LABELS[ASTRONOMICAL_ROW_COUNT] = {
    "Sunrise",
    "Sunset",
    "Moonrise",
    "Moonset",
    "Moon Phase",
};

static String astronomicalValues[ASTRONOMICAL_ROW_COUNT];
static String renderedAstronomicalValues[ASTRONOMICAL_ROW_COUNT];
static String daylightValue;
static String renderedDaylightValue;
static bool forceAstronomicalRender = true;
static String lastAstronomicalPayload;

static TextTablePage astronomicalPage(const TextTableRow *rows)
{
    return {
        "Astronomical",
        CYD_NAVY,
        150,
        rows,
        rows ? ASTRONOMICAL_DISPLAY_ROW_COUNT : 0,
        enterDetailsScreen,
        {nullptr, nullptr},
        {nullptr, nullptr},
    };
}

static void updateAstronomicalRows(bool redrawPage)
{
    bool changed = forceAstronomicalRender;
    AstronomicalPageValues pageValues;
    int daylightMinutes = 0;
    String payload;
    const bool available = adafruitIOManager.getFeedValue(ASTRONOMICAL_FEED_KEYS[0], payload);
    const char *error = available
        ? parseAstronomicalPageValues(payload.c_str(), pageValues, daylightMinutes) : nullptr;
    if (error && (forceAstronomicalRender || payload != lastAstronomicalPayload))
        Serial.printf("Astronomical: invalid %s: %s.\n", ASTRONOMICAL_FEED_KEYS[0], error);
    lastAstronomicalPayload = payload;
    for (size_t i = 0; i < ASTRONOMICAL_ROW_COUNT; ++i)
    {
        astronomicalValues[i] = available && !error
            ? String(pageValues[i].data()) : String("--");
        if (astronomicalValues[i].isEmpty())
            astronomicalValues[i] = "--";
        changed = changed || astronomicalValues[i] != renderedAstronomicalValues[i];
    }
    char daylight[6];
    snprintf(daylight, sizeof(daylight), "%d:%02d", daylightMinutes / 60, daylightMinutes % 60);
    daylightValue = available && !error ? String(daylight) : String("--");
    changed = changed || daylightValue != renderedDaylightValue;
    if (!changed)
        return;

    TextTableRow rows[ASTRONOMICAL_DISPLAY_ROW_COUNT] = {};
    for (size_t i = 0; i < ASTRONOMICAL_ROW_COUNT; ++i)
    {
        const size_t displayRow = i + (i >= 2 ? 2 : 0) + (i >= 4 ? 1 : 0);
        rows[displayRow] = {
            ASTRONOMICAL_LABELS[i],
            astronomicalValues[i].c_str(),
            i < 2 ? TextTableColor::Yellow : TextTableColor::Cyan,
            TextTableColor::White,
        };
    }
    rows[2] = {
        "Daylight",
        daylightValue.c_str(),
        TextTableColor::Yellow,
        TextTableColor::White,
    };

    const TextTablePage page = astronomicalPage(rows);
    const bool rendered = redrawPage
        ? drawTextTablePage(page)
        : refreshTextTablePageRows(page);
    if (rendered)
    {
        for (size_t i = 0; i < ASTRONOMICAL_ROW_COUNT; ++i)
            renderedAstronomicalValues[i] = astronomicalValues[i];
        renderedDaylightValue = daylightValue;
        forceAstronomicalRender = false;
    }
}

void drawAstronomicalScreen()
{
    if (!adafruitIOManager.setActiveFeeds(
            ASTRONOMICAL_FEED_KEYS, 1))
    {
        Serial.println("Astronomical: could not activate pages.p05-astro.");
    }
    forceAstronomicalRender = true;
    updateAstronomicalRows(true);
}

void enterAstronomicalScreen()
{
    state.currentScreen = SCREEN_ASTRONOMICAL;
    drawCurrentScreen();
}

void handleAstronomicalTouch(int x, int y)
{
    handleTextTablePageTouch(astronomicalPage(nullptr), x, y);
}

void refreshAstronomicalResults()
{
    updateAstronomicalRows(false);
}
