#include "app.h"
#include "screens/history_chart_page.h"

static HistoryChartScale temperatureHistoryScale(double, double)
{
    return {5.0, 10.0};
}

static const HistoryChartSeries TEMPERATURE_HISTORY_SERIES[] = {
    {"feed-avg", HistoryAggregation::Average, CYD_CYAN, nullptr},
};

static HistoryChartPage temperatureHistoryPage()
{
    return {
        "Last 24 Hours",
        TEMPERATURE_HISTORY_SERIES,
        1,
        86400,
        3600,
        "24 hours",
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
    refreshHistoryChartPage(temperatureHistoryPage());
}

void drawTemperatureHistoryScreen()
{
    drawHistoryChartPage(temperatureHistoryPage());
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
}
