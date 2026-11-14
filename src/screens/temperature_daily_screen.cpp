#include "app.h"
#include "screens/snapshot_history_page.h"

static HistoryChartScale dailyTemperatureScale(double, double)
{
    return {5.0, 10.0};
}

static const HistoryChartSeries DAILY_TEMPERATURE_SERIES[] = {
    {"pages.om-temp", HistoryAggregation::Minimum, CYD_CYAN, "Low"},
    {"pages.om-temp", HistoryAggregation::Maximum, CYD_CYAN, "High"},
};
static const SnapshotHistoryField DAILY_TEMPERATURE_FIELDS[] = {
    {"t2m_min", HistoryAggregation::Minimum},
    {"t2m_max", HistoryAggregation::Maximum},
};

static HistoryChartPage dailyTemperaturePage()
{
    return {
        "Daily Temps - 28 Days",
        DAILY_TEMPERATURE_SERIES,
        2,
        28U * 86400U,
        86400,
        "28 days",
        " F",
        1,
        10.0,
        false,
        dailyTemperatureScale,
        HistoryChartStyle::DailyForecastFloatingBars,
    };
}

void refreshTemperatureDailyResults()
{
    refreshSnapshotHistoryPage(dailyTemperaturePage());
}

void drawTemperatureDailyScreen()
{
    drawSnapshotHistoryPage(dailyTemperaturePage(), "pages.om-temp",
                            DAILY_TEMPERATURE_FIELDS, 32.0);
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
