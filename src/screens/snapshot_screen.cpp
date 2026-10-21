#include "app.h"

#include <math.h>
#include <stdlib.h>

#include "adafruitio/adafruitio_manager.h"
#include "screens/text_table_page.h"

static const char *const SNAPSHOT_FEED_KEYS[] = {
    "currentconditioncode",
    "feed-avg",
    "currenthumidity",
    "currentpressure",
    "currentpressuretrend",
    "currentuv",
    "currentcloudcover",
    "currentwinddirection",
    "currentwindspeed",
    "currentwindgust",
    "m5precipitationchance",
    "hours1precipitationchance",
    "hours2precipitationchance",
    "hours6precipitationchance",
    "hours12precipitationchance",
};

static const int SNAPSHOT_VALUE_COLUMN_X = 140;
static const size_t SNAPSHOT_ROW_COUNT = 8;

static const char *const SNAPSHOT_LABELS[SNAPSHOT_ROW_COUNT] = {
    "Conditions",
    "Temperature",
    "Humidity",
    "Barometer",
    "UV / Clouds",
    "Wind",
    "Wind / Gust",
    "Rain Chance",
};

static const TextTableColor SNAPSHOT_LABEL_COLORS[SNAPSHOT_ROW_COUNT] = {
    TextTableColor::Yellow,
    TextTableColor::White,
    TextTableColor::Pink,
    TextTableColor::Pink,
    TextTableColor::Yellow,
    TextTableColor::Cyan,
    TextTableColor::Cyan,
    TextTableColor::Pink,
};

static String snapshotValues[SNAPSHOT_ROW_COUNT];
static String lastRenderedValues[SNAPSHOT_ROW_COUNT];
static bool forceSnapshotRender = true;

static String readSnapshotFeed(const char *feedKey)
{
    String value;
    if (!adafruitIOManager.getFeedValue(feedKey, value))
        return "--";
    value.trim();
    return value.isEmpty() ? String("--") : value;
}

static bool parseSnapshotNumber(const String &value, float &number)
{
    char *end = nullptr;
    number = strtof(value.c_str(), &end);
    return end != value.c_str() && isfinite(number);
}

static String readSnapshotTemperature()
{
    String value;
    if (!adafruitIOManager.getFeedValue("feed-avg", value))
        return "-- F";

    return formatTemperatureFeedValue(value);
}

static String readSnapshotPressure()
{
    String value;
    if (!adafruitIOManager.getFeedValue("currentpressure", value))
        return "--";

    float pressure;
    if (!parseSnapshotNumber(value, pressure))
        return "--";
    return String(pressure, 1);
}

static String readSnapshotRainChance()
{
    static const char *const feedKeys[] = {
        "m5precipitationchance",
        "hours1precipitationchance",
        "hours2precipitationchance",
        "hours6precipitationchance",
        "hours12precipitationchance",
    };
    static const char *const timeFrames[] = {
        "Now",
        "Soon",
        "Soon",
        "Later",
        "Today",
    };
    static const size_t feedCount = sizeof(feedKeys) / sizeof(feedKeys[0]);
    String values[feedCount];
    float chances[feedCount];

    size_t earliestNonzero = feedCount;
    for (size_t i = 0; i < feedCount; ++i)
    {
        values[i] = readSnapshotFeed(feedKeys[i]);
        if (!parseSnapshotNumber(values[i], chances[i]))
            return "--";
        if (earliestNonzero == feedCount && chances[i] > 0.0f)
            earliestNonzero = i;
    }

    if (earliestNonzero == feedCount)
        return "0%";

    String result = values[earliestNonzero];
    if (!values[earliestNonzero].endsWith("%"))
        result += "%";
    result += " ";
    result += timeFrames[earliestNonzero];

    size_t laterMaximum = feedCount;
    for (size_t i = earliestNonzero + 1; i < feedCount; ++i)
    {
        if (chances[i] > chances[earliestNonzero] &&
            (laterMaximum == feedCount || chances[i] > chances[laterMaximum]))
        {
            laterMaximum = i;
        }
    }

    if (laterMaximum != feedCount)
    {
        result += "-";
        result += values[laterMaximum];
        if (!values[laterMaximum].endsWith("%"))
            result += "%";
        result += " ";
        result += timeFrames[laterMaximum];
    }

    return result;
}

static bool updateSnapshotRows(bool redrawPage)
{
    snapshotValues[0] = readSnapshotFeed("currentconditioncode");
    snapshotValues[1] = readSnapshotTemperature();

    snapshotValues[2] = readSnapshotFeed("currenthumidity");
    if (snapshotValues[2] != "--")
        snapshotValues[2] += " %";

    snapshotValues[3] = readSnapshotPressure();
    if (snapshotValues[3] != "--")
    {
        snapshotValues[3] += " hPa";
        const String trend = readSnapshotFeed("currentpressuretrend");
        if (trend != "--")
            snapshotValues[3] += " " + trend;
    }

    snapshotValues[4] = readSnapshotFeed("currentuv");
    const String cloudCover = readSnapshotFeed("currentcloudcover");
    if (cloudCover != "--")
        snapshotValues[4] += " - " + cloudCover + " %";

    snapshotValues[5] = readSnapshotFeed("currentwinddirection");

    const String windSpeed = readSnapshotFeed("currentwindspeed");
    const String windGust = readSnapshotFeed("currentwindgust");
    snapshotValues[6] = windSpeed + " / " + windGust;
    if (windSpeed != "--" || windGust != "--")
        snapshotValues[6] += " mph";

    snapshotValues[7] = readSnapshotRainChance();

    bool changed = forceSnapshotRender;
    for (size_t i = 0; i < SNAPSHOT_ROW_COUNT; ++i)
        changed = changed || snapshotValues[i] != lastRenderedValues[i];
    if (!changed)
        return true;

    TextTableRow rows[SNAPSHOT_ROW_COUNT];
    for (size_t i = 0; i < SNAPSHOT_ROW_COUNT; ++i)
    {
        rows[i] = {
            SNAPSHOT_LABELS[i],
            snapshotValues[i].c_str(),
            SNAPSHOT_LABEL_COLORS[i],
            TextTableColor::White,
        };
    }

    const TextTablePage page = {
        "Snapshot",
        CYD_NAVY,
        SNAPSHOT_VALUE_COLUMN_X,
        rows,
        SNAPSHOT_ROW_COUNT,
        enterHomeScreen,
        {"Summary", enterSummaryScreen},
        {nullptr, nullptr},
    };

    const bool rendered = redrawPage
        ? drawTextTablePage(page)
        : refreshTextTablePageRows(page);
    if (rendered)
    {
        for (size_t i = 0; i < SNAPSHOT_ROW_COUNT; ++i)
            lastRenderedValues[i] = snapshotValues[i];
        forceSnapshotRender = false;
    }
    return rendered;
}

void drawSnapshotScreen()
{
    if (!adafruitIOManager.setActiveFeeds(
            SNAPSHOT_FEED_KEYS,
            sizeof(SNAPSHOT_FEED_KEYS) / sizeof(SNAPSHOT_FEED_KEYS[0])))
    {
        Serial.println("Snapshot: could not activate its Adafruit IO feeds.");
    }
    forceSnapshotRender = true;
    updateSnapshotRows(true);
}

void enterSnapshotScreen()
{
    state.currentScreen = SCREEN_SNAPSHOT;
    drawCurrentScreen();
}

void handleSnapshotTouch(int x, int y)
{
    const TextTablePage page = {
        "Snapshot",
        CYD_NAVY,
        SNAPSHOT_VALUE_COLUMN_X,
        nullptr,
        0,
        enterHomeScreen,
        {"Summary", enterSummaryScreen},
        {nullptr, nullptr},
    };
    handleTextTablePageTouch(page, x, y);
}

void refreshSnapshotResults()
{
    updateSnapshotRows(false);
}
