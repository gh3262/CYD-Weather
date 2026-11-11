#include "app.h"
#include "screens/snapshot_history_page.h"

static const HistoryChartSeries CLOUDS_SERIES[] = {
    {"pages.om-sun", HistoryAggregation::Minimum, CYD_GRAY, "Cloud cover"},
    {"pages.om-sun", HistoryAggregation::Maximum, CYD_GRAY, "Cloud cover"},
    {"pages.om-sun", HistoryAggregation::Average, CYD_YELLOW, "Sunshine"},
};

static HistoryChartScale cloudsScale(double, double)
{
    return {10, 20};
}

static HistoryChartPage cloudsPage()
{
    return {
        "Clouds", CLOUDS_SERIES, 3, 28U * 86400U, 86400,
        "28 days", "%", 0, 0, true, cloudsScale, HistoryChartStyle::CloudRangeLine,
    };
}

void refreshCloudsResults()
{
    refreshSnapshotHistoryPage(cloudsPage());
}

void drawCloudsScreen()
{
    drawSnapshotHistoryPage(cloudsPage(), "pages.om-sun", nullptr);
}

void enterCloudsScreen()
{
    state.currentScreen = SCREEN_CLOUDS;
    drawCurrentScreen();
}

void handleCloudsTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterDetailsScreen();
}
