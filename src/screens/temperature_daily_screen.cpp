#include "app.h"
#include "screens/history_chart_page.h"

static HistoryChartScale dailyTemperatureScale(double, double)
{
    return {5.0, 10.0};
}

static const HistoryChartSeries DAILY_TEMPERATURE_SERIES[] = {
    {"omp-mintemp", HistoryAggregation::Minimum, CYD_CYAN, "Low"},
    {"omp-maxtemp", HistoryAggregation::Maximum, CYD_CYAN, "High"},
};

static HistoryChartPage dailyTemperaturePage()
{
    return {
        "Daily Temps - 14 Days",
        DAILY_TEMPERATURE_SERIES,
        2,
        14U * 86400U,
        86400,
        "14 days",
        " F",
        1,
        10.0,
        false,
        dailyTemperatureScale,
        HistoryChartStyle::DailyFloatingBars,
    };
}

void refreshTemperatureDailyResults()
{
    refreshHistoryChartPage(dailyTemperaturePage());
}

void drawTemperatureDailyScreen()
{
    drawHistoryChartPage(dailyTemperaturePage());
    drawNav3("History");
}

void enterTemperatureDailyScreen()
{
    state.currentScreen = SCREEN_TEMPERATURE_DAILY;
    drawCurrentScreen();
}

void handleTemperatureDailyTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterTemperaturesScreen();
    else if (navPressed3(x, y))
        enterTemperatureHistoryScreen();
}
