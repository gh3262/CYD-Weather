#include "app.h"

#include <math.h>
#include <time.h>
#include <vector>

#include "adafruitio/adafruitio_manager.h"
#include "widgets/line_chart_widget.h"

static const char *const HISTORY_FEEDS[] = {"feed-avg"};
static bool historyActive = false;
static uint32_t renderedRevision = UINT32_MAX;
static bool renderedStale = false;
static bool renderedHasData = false;

static void drawHistoryMessage(const char *message)
{
    gfx->fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, NAV_Y - HEADER_HEIGHT,
                  COLOR_BACKGROUND);
    setDefaultFont();
    centerText(message, 105, COLOR_TEXT, 1);
    gfx->setTextSize(2);
}

static void drawHistoryTimes(const NumericHistoryResult &history)
{
    const time_t first = static_cast<time_t>(history.startUtc);
    const time_t last = static_cast<time_t>(history.endUtc);
    struct tm local;
    char label[24];
    gfx->setCursor(4, NAV_Y - 11);
    if (localtime_r(&first, &local) && strftime(label, sizeof(label), "%m/%d %H:%M", &local))
        gfx->print(label);
    gfx->setCursor(SCREEN_WIDTH - 76, NAV_Y - 11);
    if (localtime_r(&last, &local) && strftime(label, sizeof(label), "%m/%d %H:%M", &local))
        gfx->print(label);
}

void refreshTemperatureHistoryResults()
{
    if (!historyActive)
        return;
    NumericHistoryResult history;
    NumericHistoryStatus status;
    const bool hasData = adafruitIOManager.getNumericHistory(history, status);
    const bool stale = status.state == NumericHistoryState::Failed ||
        WiFi.status() != WL_CONNECTED ||
        (hasData && static_cast<int64_t>(time(nullptr)) > history.endUtc + 120);
    if (status.revision == renderedRevision && stale == renderedStale &&
        hasData == renderedHasData)
        return;

    if (!hasData)
    {
        drawHistoryMessage(status.state == NumericHistoryState::Failed
            ? "History failed - retrying" : stale
                ? "Offline - waiting for WiFi" : "Loading history...");
    }
    else if (history.sampleCount == 0)
    {
        drawHistoryMessage(stale ? "No readings (stale history)" :
            history.invalidValueCount ? "No valid readings in last 24h" :
                "No readings in last 24 hours");
    }
    else
    {
        std::vector<LineChartPoint> points;
        points.reserve(history.buckets.size());
        for (const NumericHistoryBucket &bucket : history.buckets)
        {
            const int64_t start = max(bucket.startUtc, history.startUtc);
            const int64_t end = min(
                bucket.startUtc + history.bucketSeconds, history.endUtc);
            points.push_back({
                start + (end - start) / 2.0,
                bucket.value(history.aggregation),
            });
        }
        LineChartWidgetOptions chart;
        chart.y = HEADER_HEIGHT + 16;
        chart.height = NAV_Y - chart.y - 14;
        chart.minimumX = static_cast<double>(history.startUtc);
        chart.maximumX = static_cast<double>(history.endUtc);
        chart.minimumY = history.minimum - 10.0;
        chart.maximumY = history.maximum + 10.0;
        chart.showYScale = true;
        chart.yTickInterval = 5.0;
        chart.yLabelInterval = 10.0;
        if (!drawLineChartWidget(chart, points.data(), points.size()))
        {
            drawHistoryMessage("Could not draw history");
            return;
        }
        gfx->fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, 16, COLOR_BACKGROUND);
        gfx->fillRect(0, NAV_Y - 14, SCREEN_WIDTH, 14, COLOR_BACKGROUND);
        setDefaultFont();
        gfx->setTextSize(1);
        gfx->setTextColor(COLOR_TEXT);
        gfx->setCursor(4, HEADER_HEIGHT + 3);
        gfx->print("Min " + String(history.minimum, 1) + " F  Max " +
                   String(history.maximum, 1) + " F");
        if (stale)
        {
            gfx->setTextColor(COLOR_WARNING);
            gfx->setCursor(SCREEN_WIDTH - 34, HEADER_HEIGHT + 3);
            gfx->print("Stale");
            gfx->setTextColor(COLOR_TEXT);
        }
        drawHistoryTimes(history);
        const String count = String(history.sampleCount) + " pts";
        centerText(count.c_str(), NAV_Y - 11, COLOR_TEXT, 1);
        gfx->setTextSize(2);
    }
    renderedRevision = status.revision;
    renderedStale = stale;
    renderedHasData = hasData;
}

void drawTemperatureHistoryScreen()
{
    historyActive = true;
    renderedRevision = UINT32_MAX;
    NumericHistoryRequest request;
    const bool requested = adafruitIOManager.requestNumericHistory("feed-avg", request);
    const bool activated = adafruitIOManager.setActiveFeeds(HISTORY_FEEDS, 1);
    clearScreen(COLOR_BACKGROUND);
    titleBar("Last 24 Hours");
    drawNav1("Back");
    drawNav4("Home");
    if (!requested || !activated)
    {
        Serial.println("Temperature history: could not request Adafruit IO history.");
        adafruitIOManager.cancelNumericHistory();
        historyActive = false;
        drawHistoryMessage("History unavailable");
        return;
    }
    drawHistoryMessage("Loading history...");
    refreshTemperatureHistoryResults();
}

void leaveTemperatureHistoryScreen()
{
    if (!historyActive)
        return;
    adafruitIOManager.cancelNumericHistory();
    historyActive = false;
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
