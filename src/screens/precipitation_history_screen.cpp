#include "app.h"
#include "screens/history_chart_page.h"

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
    {"omp-totprec", HistoryAggregation::Maximum, CYD_CYAN, nullptr},
};

static HistoryChartPage precipitationHistoryPage()
{
    return {
        "Precip - 14 Days",
        PRECIPITATION_HISTORY_SERIES,
        1,
        14U * 86400U,
        86400,
        "14 days",
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
    refreshHistoryChartPage(precipitationHistoryPage());
}

void drawPrecipitationHistoryScreen()
{
    drawHistoryChartPage(precipitationHistoryPage());
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
