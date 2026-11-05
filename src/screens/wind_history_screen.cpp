#include "app.h"
#include "screens/snapshot_history_page.h"

// Speed is drawn first so the gust line stays visible where they meet.
static const HistoryChartSeries WIND_HISTORY_SERIES[] = {
    {"pages.wind-line", HistoryAggregation::Average, CYD_CYAN, "Speed"},
    {"pages.wind-line", HistoryAggregation::Maximum, CYD_YELLOW, "Gust"},
};
static const SnapshotHistoryField WIND_HISTORY_FIELDS[] = {
    {"speed", HistoryAggregation::Average, false},
    {"gust", HistoryAggregation::Maximum, true},
};

// Chooses the finest scale with at most eight Y labels.
static HistoryChartScale windHistoryScale(double minimumY, double maximumY)
{
    static const HistoryChartScale SCALES[] = {
        {1.0, 2.0}, {1.0, 5.0}, {5.0, 10.0}, {5.0, 20.0}, {10.0, 50.0},
    };
    const double range = maximumY - minimumY;
    for (const HistoryChartScale &scale : SCALES)
    {
        if (range / scale.labelInterval <= 8.0)
            return scale;
    }
    return {50.0, 100.0};
}

static HistoryChartPage windHistoryPage()
{
    return {
        "Last 72 Hours",
        WIND_HISTORY_SERIES,
        sizeof(WIND_HISTORY_SERIES) / sizeof(WIND_HISTORY_SERIES[0]),
        72U * 3600U,
        3600,
        "72 hours",
        " mph",
        0,
        2.0,
        true,
        windHistoryScale,
        HistoryChartStyle::Line,
    };
}

void refreshWindHistoryResults()
{
    refreshSnapshotHistoryPage(windHistoryPage());
}

void drawWindHistoryScreen()
{
    drawSnapshotHistoryPage(windHistoryPage(), "pages.wind-line", WIND_HISTORY_FIELDS);
    drawNav2("Dir");
}

void enterWindHistoryScreen()
{
    state.currentScreen = SCREEN_WIND_HISTORY;
    drawCurrentScreen();
}

void handleWindHistoryTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterWindScreen();
    else if (navPressed2(x, y))
        enterWindDirectionScreen();
}
