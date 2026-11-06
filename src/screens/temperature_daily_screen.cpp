#include "app.h"
#include "screens/snapshot_history_page.h"

static HistoryChartScale dailyTemperatureScale(double, double)
{
    return {5.0, 10.0};
}

static const HistoryChartSeries DAILY_TEMPERATURE_SERIES[] = {
    {"pages.dailytemp-bar", HistoryAggregation::Minimum, CYD_CYAN, "Low"},
    {"pages.dailytemp-bar", HistoryAggregation::Maximum, CYD_CYAN, "High"},
};
static const SnapshotHistoryField DAILY_TEMPERATURE_FIELDS[] = {
    {"mintemp", HistoryAggregation::Minimum},
    {"maxtemp", HistoryAggregation::Maximum},
};

static HistoryChartPage dailyTemperaturePage()
{
    return {
        "Daily Temps - 30 Days",
        DAILY_TEMPERATURE_SERIES,
        2,
        30U * 86400U,
        86400,
        "30 days",
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
    refreshSnapshotHistoryPage(dailyTemperaturePage());
}

void drawTemperatureDailyScreen()
{
    drawSnapshotHistoryPage(dailyTemperaturePage(), "pages.dailytemp-bar",
                            DAILY_TEMPERATURE_FIELDS);
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
