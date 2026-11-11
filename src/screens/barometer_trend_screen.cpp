#include "app.h"
#include "screens/barometer_history_scale.h"
#include "screens/snapshot_history_page.h"

static const HistoryChartSeries PRESSURE_TREND_SERIES[] = {
    {"pages.baro-trend", HistoryAggregation::Minimum, CYD_GREEN, "Low"},
    {"pages.baro-trend", HistoryAggregation::Maximum, CYD_GREEN, "High"},
    {"pages.baro-trend", HistoryAggregation::Average, COLOR_TEXT, "Average"},
};
static const SnapshotHistoryField PRESSURE_TREND_FIELDS[] = {
    {"min", HistoryAggregation::Minimum},
    {"max", HistoryAggregation::Maximum},
    {"average", HistoryAggregation::Average},
};

static HistoryChartPage pressureTrendPage()
{
    return {
        "Barometer Trend",
        PRESSURE_TREND_SERIES,
        3,
        72U * 3600U,
        3U * 3600U,
        "3 days",
        " hPa",
        1,
        1.0,
        false,
        pressureHistoryScale,
        HistoryChartStyle::PressureTrendBars,
    };
}

void refreshBarometerTrendResults()
{
    refreshSnapshotHistoryPage(pressureTrendPage());
}

void drawBarometerTrendScreen()
{
    drawSnapshotHistoryPage(pressureTrendPage(), "pages.baro-trend", PRESSURE_TREND_FIELDS);
    drawNav3("History");
}

void enterBarometerTrendScreen()
{
    state.currentScreen = SCREEN_BAROMETER_TREND;
    drawCurrentScreen();
}

void handleBarometerTrendTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterBarometerScreen();
    else if (navPressed3(x, y))
        enterBarometerHistoryScreen();
}
