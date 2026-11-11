#include "app.h"
#include "screens/snapshot_history_page.h"
#include "screens/barometer_history_scale.h"

static const HistoryChartSeries PRESSURE_HISTORY_SERIES[] = {
    {"pages.baro-line", HistoryAggregation::Average, CYD_CYAN, nullptr},
};
static const SnapshotHistoryField PRESSURE_HISTORY_FIELDS[] = {
    {"baro", HistoryAggregation::Average},
};

static HistoryChartPage pressureHistoryPage()
{
    return {
        "Last 72 Hours",
        PRESSURE_HISTORY_SERIES,
        1,
        72U * 3600U,
        3600,
        "72 hours",
        " hPa",
        1,
        2.0,
        false,
        pressureHistoryScale,
        HistoryChartStyle::Line,
    };
}

void refreshBarometerHistoryResults()
{
    refreshSnapshotHistoryPage(pressureHistoryPage());
}

void drawBarometerHistoryScreen()
{
    drawSnapshotHistoryPage(pressureHistoryPage(), "pages.baro-line", PRESSURE_HISTORY_FIELDS);
    drawNav3("Trend");
}

void enterBarometerHistoryScreen()
{
    state.currentScreen = SCREEN_BAROMETER_HISTORY;
    drawCurrentScreen();
}

void handleBarometerHistoryTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterBarometerScreen();
    else if (navPressed3(x, y))
        enterBarometerTrendScreen();
}
