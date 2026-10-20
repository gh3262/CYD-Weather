#include "app.h"
#include "screens/history_chart_page.h"

// Chooses the finest scale with at most eight Y labels so large swings fit.
static HistoryChartScale pressureHistoryScale(double minimumY, double maximumY)
{
    static const HistoryChartScale SCALES[] = {
        {0.5, 1.0}, {1.0, 2.0}, {1.0, 5.0}, {5.0, 10.0}, {10.0, 20.0}, {10.0, 50.0},
    };
    const double range = maximumY - minimumY;
    for (const HistoryChartScale &scale : SCALES)
    {
        if (range / scale.labelInterval <= 8.0)
            return scale;
    }
    return {50.0, 100.0};
}

static const HistoryChartSeries PRESSURE_HISTORY_SERIES[] = {
    {"currentpressure", HistoryAggregation::Average, CYD_CYAN, nullptr},
};

static HistoryChartPage pressureHistoryPage()
{
    return {
        "Last 72 Hours",
        PRESSURE_HISTORY_SERIES,
        1,
        72U * 3600U,
        2U * 3600U,
        "72 hours",
        " hPa",
        1,
        2.0,
        false,
        pressureHistoryScale,
    };
}

void refreshBarometerHistoryResults()
{
    refreshHistoryChartPage(pressureHistoryPage());
}

void drawBarometerHistoryScreen()
{
    drawHistoryChartPage(pressureHistoryPage());
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
}
