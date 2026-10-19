#include "app.h"

#include "adafruitio/adafruitio_manager.h"
#include "screens/text_table_page.h"

static const char *const ASTRONOMICAL_FEED_KEYS[] = {
    "days1sunrise",
    "days1sunset",
    "days1moonrise",
    "days1moonset",
    "days1moonphase",
};

static const size_t ASTRONOMICAL_ROW_COUNT =
    sizeof(ASTRONOMICAL_FEED_KEYS) / sizeof(ASTRONOMICAL_FEED_KEYS[0]);
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

static String readDaylightValue()
{
    String sunriseValue;
    String sunsetValue;
    if (!adafruitIOManager.getFeedValue("days1sunrise", sunriseValue) ||
        !adafruitIOManager.getFeedValue("days1sunset", sunsetValue))
        return "--";

    sunriseValue.trim();
    sunsetValue.trim();
    time_t sunrise;
    time_t sunset;
    struct tm localSunrise;
    struct tm localSunset;
    if (!parseUtcTimestamp(sunriseValue.c_str(), sunrise) ||
        !parseUtcTimestamp(sunsetValue.c_str(), sunset) ||
        localtime_r(&sunrise, &localSunrise) == nullptr ||
        localtime_r(&sunset, &localSunset) == nullptr)
        return "--";

    // The "next" events may be on different dates; compare local clock times.
    const int sunriseSeconds = localSunrise.tm_hour * 3600 +
        localSunrise.tm_min * 60 + localSunrise.tm_sec;
    const int sunsetSeconds = localSunset.tm_hour * 3600 +
        localSunset.tm_min * 60 + localSunset.tm_sec;
    const int daylightMinutes =
        ((sunsetSeconds - sunriseSeconds + 86400) % 86400) / 60;
    char buffer[6];
    snprintf(buffer, sizeof(buffer), "%d:%02d",
             daylightMinutes / 60, daylightMinutes % 60);
    return String(buffer);
}

static String readAstronomicalValue(const char *feedKey)
{
    String value;
    if (!adafruitIOManager.getFeedValue(feedKey, value))
        return "--";

    value.trim();
    if (value.isEmpty())
        return "--";

    if (strcmp(feedKey, "days1moonphase") == 0)
        return String(moonPhaseDisplayName(value.c_str()));

    return String(formatUtcTimestampLocal(value.c_str()));
}

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
    for (size_t i = 0; i < ASTRONOMICAL_ROW_COUNT; ++i)
    {
        astronomicalValues[i] = readAstronomicalValue(ASTRONOMICAL_FEED_KEYS[i]);
        changed = changed || astronomicalValues[i] != renderedAstronomicalValues[i];
    }
    daylightValue = readDaylightValue();
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
            ASTRONOMICAL_FEED_KEYS, ASTRONOMICAL_ROW_COUNT))
    {
        Serial.println("Astronomical: could not activate its Adafruit IO feeds.");
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
