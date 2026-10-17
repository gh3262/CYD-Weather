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
    "currentuv",
    "currentwindspeed",
    "currentwindgust",
    "currentwinddirection",
};

static const char *const SNAPSHOT_LABELS[] = {
    "Conditions",
    "Temperature",
    "Humidity",
    "Barometer",
    "UV Index",
    "Wind",
    "Gusting",
    "Wind Direction",
};

static const TextTableColor SNAPSHOT_LABEL_COLORS[] = {
    TextTableColor::Yellow,
    TextTableColor::White,
    TextTableColor::Pink,
    TextTableColor::Pink,
    TextTableColor::Yellow,
    TextTableColor::Cyan,
    TextTableColor::Cyan,
    TextTableColor::Cyan,
};

static String snapshotValues[8];
static String lastRenderedValues[8];
static bool forceSnapshotRender = true;

static String readSnapshotFeed(const char *feedKey)
{
    String value;
    if (!adafruitIOManager.getFeedValue(feedKey, value))
        return "--";
    return value;
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

    float temperature;
    if (!parseSnapshotNumber(value, temperature))
        return "-- F";

    return String(static_cast<long>(lroundf(temperature))) + " F";
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

static bool updateSnapshotRows(bool redrawPage)
{
    snapshotValues[0] = readSnapshotFeed(SNAPSHOT_FEED_KEYS[0]);
    snapshotValues[1] = readSnapshotTemperature();
    for (size_t i = 2; i < 8; ++i)
        snapshotValues[i] = readSnapshotFeed(SNAPSHOT_FEED_KEYS[i]);

    if (snapshotValues[2] != "--")
        snapshotValues[2] += " %";
    snapshotValues[3] = readSnapshotPressure();
    if (snapshotValues[3] != "--")
        snapshotValues[3] += " hPa";
    if (snapshotValues[5] != "--")
        snapshotValues[5] += " mph";
    if (snapshotValues[6] != "--")
        snapshotValues[6] += " mph";

    bool changed = forceSnapshotRender;
    for (size_t i = 0; i < 8; ++i)
        changed = changed || snapshotValues[i] != lastRenderedValues[i];
    if (!changed)
        return true;

    TextTableRow rows[8];
    for (size_t i = 0; i < 8; ++i)
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
        160,
        rows,
        8,
        enterHomeScreen,
        {nullptr, nullptr},
        {nullptr, nullptr},
    };

    const bool rendered = redrawPage
        ? drawTextTablePage(page)
        : refreshTextTablePageRows(page);
    if (rendered)
    {
        for (size_t i = 0; i < 8; ++i)
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
        160,
        nullptr,
        0,
        enterHomeScreen,
        {nullptr, nullptr},
        {nullptr, nullptr},
    };
    handleTextTablePageTouch(page, x, y);
}

void refreshSnapshotResults()
{
    updateSnapshotRows(false);
}
