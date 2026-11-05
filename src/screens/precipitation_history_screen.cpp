#include "app.h"
#include "screens/snapshot_history_page.h"

#include <math.h>

static HistoryChartScale precipitationHistoryScale(double, double maximumY)
{
    const double target = maximumY / 5.0;
    const double magnitude = pow(10.0, floor(log10(target)));
    const double fraction = target / magnitude;
    const double step = max(0.01, magnitude *
        (fraction <= 1.0 ? 1.0 : fraction <= 2.0 ? 2.0 : fraction <= 5.0 ? 5.0 : 10.0));
    return {step, step};
}

static const HistoryChartSeries PRECIPITATION_HISTORY_SERIES[] = {
    {"pages.prcp-line", HistoryAggregation::Maximum, CYD_CYAN, nullptr},
};
static const SnapshotHistoryField PRECIPITATION_HISTORY_FIELDS[] = {
    {"prcp", HistoryAggregation::Maximum, false},
};

static HistoryChartPage precipitationHistoryPage()
{
    return {
        "Precip - 30 Days",
        PRECIPITATION_HISTORY_SERIES,
        1,
        30U * 86400U,
        86400,
        "30 days",
        " in",
        3,
        0.01,
        true,
        precipitationHistoryScale,
        HistoryChartStyle::DailyBars,
    };
}

void refreshPrecipitationHistoryResults()
{
    refreshSnapshotHistoryPage(precipitationHistoryPage());
}

void drawPrecipitationHistoryScreen()
{
    drawSnapshotHistoryPage(precipitationHistoryPage(), "pages.prcp-line",
                            PRECIPITATION_HISTORY_FIELDS);
}

void enterPrecipitationHistoryScreen()
{
    state.currentScreen = SCREEN_PRECIPITATION_HISTORY;
    drawCurrentScreen();
}

void handlePrecipitationHistoryTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterPrecipitationScreen();
}
