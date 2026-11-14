#include "app.h"
#include "screens/snapshot_history_page.h"

static HistoryChartScale temperatureHistoryScale(double, double)
{
    return {5.0, 10.0};
}

static const HistoryChartSeries TEMPERATURE_HISTORY_SERIES[] = {
    {"pages.temp-line", HistoryAggregation::Average, CYD_CYAN, nullptr},
};
static const SnapshotHistoryField TEMPERATURE_HISTORY_FIELDS[] = {
    {"ftemp", HistoryAggregation::Average},
};

static HistoryChartPage temperatureHistoryPage()
{
    return {
        "Last 72 Hours",
        TEMPERATURE_HISTORY_SERIES,
        1,
        72U * 3600U,
        3600,
        "72 hours",
        " F",
        1,
        10.0,
        false,
        temperatureHistoryScale,
        HistoryChartStyle::Line,
    };
}

void refreshTemperatureHistoryResults()
{
    refreshSnapshotHistoryPage(temperatureHistoryPage());
}

void drawTemperatureHistoryScreen()
{
    drawSnapshotHistoryPage(temperatureHistoryPage(), "pages.temp-line",
                            TEMPERATURE_HISTORY_FIELDS, 32.0);
    drawNav2("Daily");
}

void enterTemperatureHistoryScreen()
{
    state.currentScreen = SCREEN_TEMPERATURE_HISTORY;
    drawCurrentScreen();
}

void handleTemperatureHistoryTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterTemperaturesScreen();
    else if (navPressed2(x, y))
        enterTemperatureDailyScreen();
}
