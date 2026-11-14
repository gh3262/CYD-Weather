#include "app.h"
#include "screens/snapshot_history_page.h"

static const HistoryChartSeries DAILY_PRECIPITATION_SERIES[] = {
    {"pages.om-precip", HistoryAggregation::Average, CYD_GRAY, "Rain"},
    {"pages.om-precip", HistoryAggregation::Average, CYD_MAGENTA, "Showers"},
    {"pages.om-precip", HistoryAggregation::Average, CYD_WHITE, "Snow"},
};

static HistoryChartScale precipitationDailyScale(double, double)
{
    return {6, 6};
}

static HistoryChartPage precipitationDailyPage()
{
    return {
        "Daily Precip - 28 Days", DAILY_PRECIPITATION_SERIES, 3,
        28U * 86400U, 86400, "28 days", " hr", 0, 0, true,
        precipitationDailyScale, HistoryChartStyle::DailyPrecipitationBars,
    };
}

void refreshPrecipitationDailyResults()
{
    refreshSnapshotHistoryPage(precipitationDailyPage());
}

void drawPrecipitationDailyScreen()
{
    drawSnapshotHistoryPage(precipitationDailyPage(), "pages.om-precip", nullptr);
}

void enterPrecipitationDailyScreen()
{
    state.currentScreen = SCREEN_PRECIPITATION_DAILY;
    drawCurrentScreen();
}

void handlePrecipitationDailyTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterPrecipitationScreen();
}
