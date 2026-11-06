#include "app.h"
#include "screens/history_chart_page.h"

#include <math.h>
#include <memory>
#include <new>
#include <time.h>
#include <vector>

#include "widgets/line_chart_widget.h"
#include "screens/weather_snapshot_history.h"

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

template <typename History>
static void drawHistoryTimes(const History &history, bool dailyBars)
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

template <typename History>
static void drawHistorySummary(const HistoryChartPage &page,
                               const History *histories,
                               double minimum, double maximum)
{
    gfx->setCursor(4, HEADER_HEIGHT + 3);
    if (page.style == HistoryChartStyle::DailyFloatingBars)
    {
        gfx->setTextColor(COLOR_TEXT);
        gfx->print("Low " + String(minimum, page.valueDecimals) +
                   page.unitSuffix + "  High " +
                   String(maximum, page.valueDecimals) + page.unitSuffix);
        return;
    }
    if (page.style == HistoryChartStyle::DailyBars)
    {
        double total = 0.0;
        double peak = 0.0;
        for (const auto &bucket : histories[0].buckets)
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
    const bool floatingBars = page.style == HistoryChartStyle::DailyFloatingBars;
    const bool daily = dailyBars || floatingBars;
    const int64_t now = static_cast<int64_t>(time(nullptr));
    int64_t dailyEnd = 0;
    const bool validDailyEnd = !daily || completedHistoryEndUtc(now, dailyEnd);
    const bool stale = status.state == NumericHistoryState::Failed ||
        WiFi.status() != WL_CONNECTED ||
        (hasData && (daily ? !validDailyEnd || dailyEnd > window.endUtc
                              : now > window.endUtc + 120));
    status.hasData = hasData;
    refreshHistoryChartData(page, histories, status, stale);
}

template <typename History>
static void refreshHistoryChartResults(const HistoryChartPage &page,
                                       const History histories[],
                                       const NumericHistoryStatus &status, bool stale)
{
    if (!historyActive)
        return;
    const bool hasData = status.hasData;
    const History &window = histories[0];
    const bool dailyBars = page.style == HistoryChartStyle::DailyBars;
    const bool floatingBars = page.style == HistoryChartStyle::DailyFloatingBars;
    const bool daily = dailyBars || floatingBars;
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

    size_t pairedDays = 0;
    if (hasData && floatingBars)
    {
        minimum = INFINITY;
        maximum = -INFINITY;
        if (histories[0].buckets.size() != histories[1].buckets.size())
        {
            Serial.println("Daily temperature history: mismatched bucket counts.");
            drawHistoryMessage("History dates do not match");
            return;
        }
        for (size_t i = 0; i < window.buckets.size(); ++i)
        {
            const auto &low = histories[0].buckets[i];
            const auto &high = histories[1].buckets[i];
            if (low.startUtc != high.startUtc)
            {
                Serial.println("Daily temperature history: mismatched bucket dates.");
                drawHistoryMessage("History dates do not match");
                return;
            }
            if (!low.count || !high.count)
                continue;
            const double lowValue = low.value(histories[0].aggregation);
            const double highValue = high.value(histories[1].aggregation);
            if (lowValue > highValue)
            {
                Serial.println("Daily temperature history: daily low exceeds high; omitting date.");
                continue;
            }
            ++pairedDays;
            minimum = min(minimum, lowValue);
            maximum = max(maximum, highValue);
        }
    }

    if (!hasData)
    {
        drawHistoryMessage(status.state == NumericHistoryState::Failed
            ? "History failed - retrying" : stale
                ? "Offline - waiting for WiFi" : "Loading history...");
    }
    else if (floatingBars && pairedDays == 0)
    {
        drawHistoryMessage(stale ? "No paired daily readings (stale)" :
                                  "No paired daily readings");
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
            for (const auto &bucket : window.buckets)
                if (bucket.count)
                    maximum = max(maximum, bucket.value(window.aggregation));
        }
        std::unique_ptr<LineChartPoint[]> points[MAX_NUMERIC_HISTORY_SERIES];
        LineChartSeries series[MAX_NUMERIC_HISTORY_SERIES];
        for (size_t s = 0; s < page.seriesCount; ++s)
        {
            const History &history = histories[s];
            points[s].reset(new (std::nothrow) LineChartPoint[history.buckets.size()]);
            if (!points[s] && !history.buckets.empty())
            {
                Serial.printf("%s: insufficient memory for chart points.\n", page.title);
                drawHistoryMessage("Not enough memory for chart");
                return;
            }
            size_t index = 0;
            for (const auto &bucket : history.buckets)
            {
                const int64_t start = max(bucket.startUtc, history.startUtc);
                const int64_t end = min(
                    bucket.startUtc + history.bucketSeconds, history.endUtc);
                points[s][index++] = {
                    start + (end - start) / 2.0,
                    dailyBars && bucket.count == 0 ? 0.0 :
                        bucket.value(history.aggregation),
                };
            }
            series[s] = {points[s].get(), history.buckets.size(), page.series[s].color};
        }
        if (floatingBars)
        {
            for (size_t i = 0; i < window.buckets.size(); ++i)
            {
                if (isnan(points[0][i].y) || isnan(points[1][i].y) ||
                    points[0][i].y > points[1][i].y)
                    points[0][i].y = points[1][i].y = NAN;
            }
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
        const bool drawn = floatingBars
            ? drawFloatingBarChartWidget(chart, points[0].get(), points[1].get(),
                                         window.buckets.size(), window.bucketSeconds * 0.75)
            : dailyBars
            ? drawBarChartWidget(chart, points[0].get(), window.buckets.size(),
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
        drawHistorySummary(page, histories, minimum, maximum);
        if (stale)
        {
            gfx->setTextColor(COLOR_WARNING);
            gfx->setCursor(SCREEN_WIDTH - 34, HEADER_HEIGHT + 3);
            gfx->print("Stale");
        }
        gfx->setTextColor(COLOR_TEXT);
        drawHistoryTimes(window, daily);
        const String count = floatingBars ? String(pairedDays) + "/" +
                String(window.buckets.size()) + " days" :
            dailyBars ? String(window.buckets.size()) + " days" :
                        String(sampleCount) + " pts";
        centerText(count.c_str(), NAV_Y - 11, COLOR_TEXT, 1);
        gfx->setTextSize(2);
    }
    renderedRevision = status.revision;
    renderedStale = stale;
    renderedHasData = hasData;
}

void refreshHistoryChartData(const HistoryChartPage &page,
                            const NumericHistoryResult histories[],
                            const NumericHistoryStatus &status, bool stale)
{
    refreshHistoryChartResults(page, histories, status, stale);
}

void refreshHistoryChartData(const HistoryChartPage &page,
                            const SnapshotHistoryResult histories[],
                            const NumericHistoryStatus &status, bool stale)
{
    refreshHistoryChartResults(page, histories, status, stale);
}

void drawHistoryChartFrame(const HistoryChartPage &page)
{
    historyActive = true;
    renderedRevision = UINT32_MAX;
    clearScreen(COLOR_BACKGROUND);
    titleBar(page.title);
    drawNav1("Back");
    drawNav4("Home");
    drawHistoryMessage("Loading history...");
}

void drawHistoryChartPage(const HistoryChartPage &page)
{
    drawHistoryChartFrame(page);
    const char *feeds[MAX_NUMERIC_HISTORY_SERIES];
    NumericHistoryRequest requests[MAX_NUMERIC_HISTORY_SERIES];
    const bool dailyBars = page.style == HistoryChartStyle::DailyBars;
    const bool floatingBars = page.style == HistoryChartStyle::DailyFloatingBars;
    const bool validPage = page.series && page.seriesCount > 0 &&
        page.seriesCount <= MAX_NUMERIC_HISTORY_SERIES &&
        (!dailyBars || (page.seriesCount == 1 && page.zeroBasedY &&
                       page.series[0].aggregation == HistoryAggregation::Maximum)) &&
        (!floatingBars || (page.seriesCount == 2 && !page.zeroBasedY &&
                          page.series[0].aggregation == HistoryAggregation::Minimum &&
                          page.series[1].aggregation == HistoryAggregation::Maximum));
    for (size_t i = 0; validPage && i < page.seriesCount; ++i)
    {
        feeds[i] = page.series[i].feedKey;
        requests[i].windowSeconds = page.windowSeconds;
        requests[i].bucketSeconds = page.bucketSeconds;
        requests[i].aggregation = page.series[i].aggregation;
        requests[i].window = dailyBars || floatingBars
            ? HistoryWindow::CompletedUtcDays : HistoryWindow::Rolling;
    }
    const bool requested = validPage &&
        adafruitIOManager.requestNumericHistory(feeds, requests, page.seriesCount);
    const bool activated = validPage &&
        adafruitIOManager.setActiveFeeds(feeds, page.seriesCount);
    if (!requested || !activated)
    {
        Serial.printf("%s: could not request Adafruit IO history.\n", page.title);
        adafruitIOManager.cancelNumericHistory();
        historyActive = false;
        drawHistoryMessage("History unavailable");
        return;
    }
    refreshHistoryChartPage(page);
}

void leaveHistoryChartPage()
{
    if (!historyActive)
        return;
    adafruitIOManager.cancelNumericHistory();
    historyActive = false;
}
