#include "app.h"
#include "screens/history_chart_page.h"

#include <math.h>
#include <time.h>
#include <vector>

#include "widgets/line_chart_widget.h"

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

static void drawHistoryTimes(const NumericHistoryResult &history, bool dailyBars)
{
    const time_t first = static_cast<time_t>(history.startUtc);
    const time_t last = static_cast<time_t>(
        dailyBars ? history.endUtc - 86400 : history.endUtc);
    struct tm local;
    char label[24];
    const char *format = dailyBars ? "%m/%d" : "%m/%d %H:%M";
    gfx->setCursor(4, NAV_Y - 11);
    if ((dailyBars ? gmtime_r(&first, &local) : localtime_r(&first, &local)) &&
        strftime(label, sizeof(label), format, &local))
        gfx->print(label);
    gfx->setCursor(SCREEN_WIDTH - (dailyBars ? 34 : 76), NAV_Y - 11);
    if ((dailyBars ? gmtime_r(&last, &local) : localtime_r(&last, &local)) &&
        strftime(label, sizeof(label), format, &local))
        gfx->print(label);
}

static void drawHistorySummary(const HistoryChartPage &page,
                               const NumericHistoryResult *histories)
{
    gfx->setCursor(4, HEADER_HEIGHT + 3);
    if (page.style == HistoryChartStyle::DailyBars)
    {
        double total = 0.0;
        double peak = 0.0;
        for (const NumericHistoryBucket &bucket : histories[0].buckets)
        {
            const double value = bucket.count ? bucket.value(histories[0].aggregation) : 0.0;
            total += value;
            peak = max(peak, value);
        }
        gfx->setTextColor(COLOR_TEXT);
        gfx->print("Total " + String(total, page.valueDecimals) +
                   page.unitSuffix + "  Peak " +
                   String(peak, page.valueDecimals) + page.unitSuffix);
        return;
    }
    if (page.seriesCount == 1)
    {
        gfx->setTextColor(COLOR_TEXT);
        gfx->print("Min " + String(histories[0].minimum, page.valueDecimals) +
                   page.unitSuffix + "  Max " +
                   String(histories[0].maximum, page.valueDecimals) + page.unitSuffix);
        return;
    }

    for (size_t i = 0; i < page.seriesCount; ++i)
    {
        gfx->setTextColor(page.series[i].color);
        gfx->print(page.series[i].label);
        gfx->print(' ');
        if (histories[i].sampleCount == 0)
            gfx->print("--");
        else
            gfx->print(String(histories[i].minimum, page.valueDecimals) + "-" +
                       String(histories[i].maximum, page.valueDecimals));
        gfx->print("  ");
    }
    gfx->setTextColor(COLOR_TEXT);
    gfx->print(String(page.unitSuffix).substring(1));
}

void refreshHistoryChartPage(const HistoryChartPage &page)
{
    if (!historyActive)
        return;
    NumericHistoryResult histories[MAX_NUMERIC_HISTORY_SERIES];
    NumericHistoryStatus status;
    const bool hasData =
        adafruitIOManager.getNumericHistory(histories, page.seriesCount, status);
    const NumericHistoryResult &window = histories[0];
    const bool dailyBars = page.style == HistoryChartStyle::DailyBars;
    const int64_t now = static_cast<int64_t>(time(nullptr));
    int64_t dailyEnd = 0;
    const bool validDailyEnd = !dailyBars || completedHistoryEndUtc(now, dailyEnd);
    const bool stale = status.state == NumericHistoryState::Failed ||
        WiFi.status() != WL_CONNECTED ||
        (hasData && (dailyBars ? !validDailyEnd || dailyEnd > window.endUtc
                              : now > window.endUtc + 120));
    if (status.revision == renderedRevision && stale == renderedStale &&
        hasData == renderedHasData)
        return;

    uint32_t sampleCount = 0;
    uint32_t invalidCount = 0;
    double minimum = INFINITY;
    double maximum = -INFINITY;
    for (size_t i = 0; hasData && i < page.seriesCount; ++i)
    {
        sampleCount += histories[i].sampleCount;
        invalidCount += histories[i].invalidValueCount;
        if (histories[i].sampleCount == 0)
            continue;
        minimum = min(minimum, histories[i].minimum);
        maximum = max(maximum, histories[i].maximum);
    }

    if (!hasData)
    {
        drawHistoryMessage(status.state == NumericHistoryState::Failed
            ? "History failed - retrying" : stale
                ? "Offline - waiting for WiFi" : "Loading history...");
    }
    else if (sampleCount == 0 && (!dailyBars || invalidCount > 0))
    {
        const String message = stale ? String("No readings (stale history)") :
            String(invalidCount ? "No valid readings in last " :
                "No readings in last ") + page.windowText;
        drawHistoryMessage(message.c_str());
    }
    else
    {
        if (dailyBars)
        {
            minimum = 0.0;
            maximum = 0.0;
            for (const NumericHistoryBucket &bucket : window.buckets)
                if (bucket.count)
                    maximum = max(maximum, bucket.value(window.aggregation));
        }
        std::vector<LineChartPoint> points[MAX_NUMERIC_HISTORY_SERIES];
        LineChartSeries series[MAX_NUMERIC_HISTORY_SERIES];
        for (size_t s = 0; s < page.seriesCount; ++s)
        {
            const NumericHistoryResult &history = histories[s];
            points[s].reserve(history.buckets.size());
            for (const NumericHistoryBucket &bucket : history.buckets)
            {
                const int64_t start = max(bucket.startUtc, history.startUtc);
                const int64_t end = min(
                    bucket.startUtc + history.bucketSeconds, history.endUtc);
                points[s].push_back({
                    start + (end - start) / 2.0,
                    dailyBars && bucket.count == 0 ? 0.0 :
                        bucket.value(history.aggregation),
                });
            }
            series[s] = {points[s].data(), points[s].size(), page.series[s].color};
        }
        LineChartWidgetOptions chart;
        chart.y = HEADER_HEIGHT + 16;
        chart.height = NAV_Y - chart.y - 14;
        chart.minimumX = static_cast<double>(window.startUtc);
        chart.maximumX = static_cast<double>(window.endUtc);
        chart.minimumY = page.zeroBasedY ? 0.0 : minimum - page.yPadding;
        chart.maximumY = maximum + page.yPadding;
        const HistoryChartScale scale = page.scale(chart.minimumY, chart.maximumY);
        chart.showYScale = true;
        chart.yTickInterval = scale.tickInterval;
        chart.yLabelInterval = scale.labelInterval;
        chart.yLabelDecimalPlaces = dailyBars ? 2 : 0;
        chart.lineColor = page.series[0].color;
        chart.zeroBarColor = CYD_YELLOW;
        const bool drawn = dailyBars
            ? drawBarChartWidget(chart, points[0].data(), points[0].size(),
                                 window.bucketSeconds * 0.75, 3)
            : drawLineChartSeriesWidget(chart, series, page.seriesCount);
        if (!drawn)
        {
            drawHistoryMessage("Could not draw history");
            return;
        }
        gfx->fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, 16, COLOR_BACKGROUND);
        gfx->fillRect(0, NAV_Y - 14, SCREEN_WIDTH, 14, COLOR_BACKGROUND);
        setDefaultFont();
        gfx->setTextSize(1);
        drawHistorySummary(page, histories);
        if (stale)
        {
            gfx->setTextColor(COLOR_WARNING);
            gfx->setCursor(SCREEN_WIDTH - 34, HEADER_HEIGHT + 3);
            gfx->print("Stale");
        }
        gfx->setTextColor(COLOR_TEXT);
        drawHistoryTimes(window, dailyBars);
        const String count = dailyBars ? String(window.buckets.size()) + " days"
                                      : String(sampleCount) + " pts";
        centerText(count.c_str(), NAV_Y - 11, COLOR_TEXT, 1);
        gfx->setTextSize(2);
    }
    renderedRevision = status.revision;
    renderedStale = stale;
    renderedHasData = hasData;
}

void drawHistoryChartPage(const HistoryChartPage &page)
{
    historyActive = true;
    renderedRevision = UINT32_MAX;
    const char *feeds[MAX_NUMERIC_HISTORY_SERIES];
    NumericHistoryRequest requests[MAX_NUMERIC_HISTORY_SERIES];
    const bool dailyBars = page.style == HistoryChartStyle::DailyBars;
    const bool validPage = page.series && page.seriesCount > 0 &&
        page.seriesCount <= MAX_NUMERIC_HISTORY_SERIES &&
        (!dailyBars || (page.seriesCount == 1 && page.zeroBasedY &&
                       page.series[0].aggregation == HistoryAggregation::Maximum));
    for (size_t i = 0; validPage && i < page.seriesCount; ++i)
    {
        feeds[i] = page.series[i].feedKey;
        requests[i].windowSeconds = page.windowSeconds;
        requests[i].bucketSeconds = page.bucketSeconds;
        requests[i].aggregation = page.series[i].aggregation;
        requests[i].window = dailyBars ? HistoryWindow::CompletedUtcDays
                                      : HistoryWindow::Rolling;
    }
    const bool requested = validPage &&
        adafruitIOManager.requestNumericHistory(feeds, requests, page.seriesCount);
    const bool activated = validPage &&
        adafruitIOManager.setActiveFeeds(feeds, page.seriesCount);
    clearScreen(COLOR_BACKGROUND);
    titleBar(page.title);
    drawNav1("Back");
    drawNav4("Home");
    if (!requested || !activated)
    {
        Serial.printf("%s: could not request Adafruit IO history.\n", page.title);
        adafruitIOManager.cancelNumericHistory();
        historyActive = false;
        drawHistoryMessage("History unavailable");
        return;
    }
    drawHistoryMessage("Loading history...");
    refreshHistoryChartPage(page);
}

void leaveHistoryChartPage()
{
    if (!historyActive)
        return;
    adafruitIOManager.cancelNumericHistory();
    historyActive = false;
}
