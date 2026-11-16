#include "app.h"
#include "screens/snapshot_history_page.h"

static const HistoryChartSeries HOURLY_TEMPERATURE_SERIES[] = {
    {"pages.om-temp-avg", HistoryAggregation::Minimum, CYD_LIGHT_GRAY, "Lower"},
    {"pages.om-temp-avg", HistoryAggregation::Maximum, CYD_LIGHT_GRAY, "Upper"},
    {"pages.om-temp-avg", HistoryAggregation::Average, CYD_CYAN, "Temperature"},
};

static HistoryChartScale hourlyTemperatureScale(double, double)
{
    return {5.0, 10.0};
}

static HistoryChartPage hourlyTemperaturePage()
{
    return {
        "Hourly Temps", HOURLY_TEMPERATURE_SERIES, 3,
        86400, 3600, "24 hours", " F", 1, 10.0, false,
        hourlyTemperatureScale, HistoryChartStyle::HourlyTemperatureBand,
    };
}

void refreshTemperatureHourlyResults()
{
    refreshSnapshotHistoryPage(hourlyTemperaturePage());
}

void drawTemperatureHourlyScreen()
{
    drawSnapshotHistoryPage(hourlyTemperaturePage(), "pages.om-temp-avg", nullptr, 32.0);
    drawNav2("Daily");
    drawNav3("History");
}

void enterTemperatureHourlyScreen()
{
    state.currentScreen = SCREEN_TEMPERATURE_HOURLY;
    drawCurrentScreen();
}

void handleTemperatureHourlyTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterTemperaturesScreen();
    else if (navPressed2(x, y))
        enterTemperatureDailyScreen();
    else if (navPressed3(x, y))
        enterTemperatureHistoryScreen();
}
