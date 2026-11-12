#include "app.h"
#include <HTTPClient.h>
#include "screens/snapshot_history_page.h"
#include "widgets/line_chart_widget.h"
#include "screens/clouds_history.h"
#include "clouds_fixture.h"
#include "temperature_daily_fixture.h"

#include <cstdlib>
#include <new>

TestSerial Serial;
TestWiFi WiFi;
uint32_t testMillis = 1000;
std::deque<TestHttpResponse> testResponses;
std::vector<std::string> testUrls;
std::function<void()> testOnGet;
AdafruitIOManager adafruitIOManager;
static TestGraphics graphics;
TestGraphics *gfx = &graphics;
static bool failAllocation = false;
static size_t draws = 0;
static size_t checks = 0;
static size_t trendDraws = 0;
static size_t cloudDraws = 0;
static double cloudDivider = NAN;
static size_t dailyForecastDraws = 0;
static double dailyDivider = NAN;

static void setTestTimezone(const char *zone)
{
#ifdef _WIN32
    _putenv_s("TZ", zone);
    _tzset();
#else
    setenv("TZ", zone, 1);
    tzset();
#endif
}

void *operator new[](size_t size, const std::nothrow_t &) noexcept
{
    if (failAllocation)
    {
        failAllocation = false;
        return nullptr;
    }
    try { return ::operator new[](size); }
    catch (const std::bad_alloc &) { return nullptr; }
}

static void check(bool condition, const char *message)
{
    ++checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

void clearScreen(uint16_t) { graphics.printed.clear(); }
void titleBar(const char *) {}
void setDefaultFont() {}
void centerText(const char *text, int, uint16_t, int) { graphics.printed.emplace_back(text); }
void drawNav1(const char *) {}
void drawNav4(const char *) {}

LineChartWidgetOptions::LineChartWidgetOptions()
    : pastForecastX(NAN), forecastBarInterval(0.0) {}
bool drawLineChartSeriesWidget(const LineChartWidgetOptions &,
                               const LineChartSeries *series, size_t count)
{
    ++draws;
    check(count == 2 && series[0].pointCount == 2 && series[1].pointCount == 2,
          "both snapshot lines retain exactly the supplied points");
    check(series[0].points[0].y == 4 && series[0].points[1].y == 5 &&
          std::isnan(series[1].points[0].y) && series[1].points[1].y == 8,
          "renderer preserves values and leading null without aggregation");
    return true;
}
bool drawBarChartWidget(const LineChartWidgetOptions &, const LineChartPoint *, size_t,
                        double, int) { ++draws; return true; }
bool drawFloatingBarChartWidget(const LineChartWidgetOptions &chart, const LineChartPoint *lows,
                                const LineChartPoint *highs, size_t count, double width)
{
    ++draws;
    if (!std::isnan(chart.pastForecastX))
    {
        ++dailyForecastDraws;
        int64_t today = 0;
        check(completedHistoryEndUtc(static_cast<int64_t>(time(nullptr)), today) &&
              chart.pastForecastX == static_cast<double>(today),
              "temperature divider uses the start of today's local date");
        dailyDivider = chart.pastForecastX;
        check(count == 28 && chart.maximumX - chart.minimumX == 28 * 86400 &&
              width == 86400 * 0.75 && chart.lineColor == CYD_CYAN &&
              chart.forecastBarInterval == 86400 &&
              std::fabs(chart.minimumY - 22.36) < 0.00001 &&
              std::fabs(chart.maximumY - 91.73) < 0.00001,
              "daily forecast retains cyan 75-percent floating bars and temperature Y scale");
        for (size_t i = 0; i < count; ++i)
            check(lows[i].x == chart.minimumX + i * 86400 + 43200 &&
                  highs[i].x == lows[i].x &&
                  std::fabs(lows[i].y - DAILY_TEMPERATURE_MIN[i]) < 0.00001 &&
                  std::fabs(highs[i].y - DAILY_TEMPERATURE_MAX[i]) < 0.00001,
                  "daily forecast renderer preserves every supplied date and temperature endpoint");
        return true;
    }
    check(count == 30 && chart.maximumX - chart.minimumX == 30 * 86400 &&
          width == 86400 * 0.75, "daily chart renders all 30 dates at 75 percent slot width");
    for (size_t i = 0; i < count; ++i)
        check(lows[i].x == chart.minimumX + i * 86400 + 43200 &&
              highs[i].x == lows[i].x && lows[i].y == static_cast<double>(i) - 5 &&
              highs[i].y == static_cast<double>(i) + 10,
              "daily floating bars preserve exact dates and low/high values including zero");
    return true;
}
static HistoryChartScale scale(double, double) { return {1, 5}; }

bool drawBarLineChartWidget(const LineChartWidgetOptions &chart,
                            const LineChartPoint *sunshine,
                            const LineChartPoint *cloudCover, size_t count,
                            double width, uint16_t lineColor)
{
    ++cloudDraws;
    int64_t today = 0;
    check(completedHistoryEndUtc(static_cast<int64_t>(time(nullptr)), today) &&
          chart.pastForecastX == static_cast<double>(today),
          "cloud divider uses local calendar midnight encoded on the date-only axis");
    cloudDivider = chart.pastForecastX;
    check(count == 28 && chart.maximumX - chart.minimumX == 28 * 86400 &&
          chart.minimumY == 0 && chart.maximumY == 100 && width == 86400 &&
          chart.lineColor == CYD_YELLOW && lineColor == CYD_RED &&
          chart.forecastBarInterval == 0,
          "Clouds uses full day yellow bars and a red line on a fixed 0-100 scale");
    for (size_t i = 0; i < count; ++i)
        check(sunshine[i].x == chart.minimumX + i * 86400 + 43200 &&
              cloudCover[i].x == sunshine[i].x &&
              std::fabs(cloudCover[i].y - CLOUDS_MEAN[i]) < 0.00001 &&
              std::fabs(sunshine[i].y - CLOUDS_SUNSHINE[i]) < 0.00001,
              "cloud renderer preserves supplied sunshine and mean cover values and centers");
    return true;
}

bool drawPressureTrendChartWidget(const LineChartWidgetOptions &chart,
                                  const LineChartPoint *lows,
                                  const LineChartPoint *highs,
                                  const LineChartPoint *averages, size_t count, double width)
{
    ++trendDraws;
    check(count == 25 && width == 10800 * 0.75 &&
          chart.maximumX - chart.minimumX == 25 * 10800,
          "trend renders all 25 supplied three-hour slots");
    for (size_t i = 0; i < count; ++i)
        check(lows[i].x == chart.minimumX + i * 10800 + 5400 &&
              highs[i].x == lows[i].x && averages[i].x == lows[i].x &&
              lows[i].y == 1018.0 + i * 0.1 &&
              highs[i].y == 1020.0 + i * 0.1 &&
              averages[i].y == 1018.25 + i * 0.1,
              "trend renderer preserves min/max/average and bucket centers");
    return true;
}

int main()
{
    static const HistoryChartSeries series[] = {
        {"pages.wind-line", HistoryAggregation::Average, CYD_CYAN, "Speed"},
        {"pages.wind-line", HistoryAggregation::Maximum, CYD_YELLOW, "Gust"},
    };
    const HistoryChartPage page = {
        "Last 72 Hours", series, 2, 72 * 3600, 3600, "72 hours", " mph",
        0, 2, true, scale, HistoryChartStyle::Line,
    };
    const SnapshotHistoryField fields[] = {
        {"speed", HistoryAggregation::Average}, {"gust", HistoryAggregation::Maximum},
    };
    static SnapshotHistoryResult histories[2];
    check(!parseWeatherSnapshotHistory(
        "[{\"ts\":\"2026110500\",\"speed\":4,\"gust\":null},"
        "{\"ts\":\"2026110501\",\"speed\":5,\"gust\":8}]", fields, 2, false, histories),
        "decode renderer fixture");
    NumericHistoryStatus status;
    status.state = NumericHistoryState::Ready;
    status.hasData = true;
    status.revision = 1;
    drawHistoryChartFrame(page);
    failAllocation = true;
    refreshHistoryChartData(page, histories, status, false);
    check(draws == 0 &&
          graphics.printed.back() == "Not enough memory for chart",
          "chart allocation failure is visible and does not abort");
    refreshHistoryChartData(page, histories, status, false);
    check(draws == 1, "same revision can redraw after memory becomes available");
    refreshHistoryChartData(page, histories, status, false);
    check(draws == 1, "unchanged chart is not redrawn");
    refreshHistoryChartData(page, histories, status, true);
    check(draws == 2 &&
          std::find(graphics.printed.begin(), graphics.printed.end(), "Stale") != graphics.printed.end(),
          "stale retained data still redraws after renderer refactor");
    NumericHistoryResult rawHistories[2];
    for (size_t s = 0; s < 2; ++s)
    {
        rawHistories[s].startUtc = histories[s].startUtc;
        rawHistories[s].endUtc = histories[s].endUtc;
        rawHistories[s].aggregation = series[s].aggregation;
        rawHistories[s].sampleCount = histories[s].sampleCount;
        rawHistories[s].minimum = histories[s].minimum;
        rawHistories[s].maximum = histories[s].maximum;
        rawHistories[s].buckets.resize(2);
        for (size_t i = 0; i < 2; ++i)
        {
            NumericHistoryBucket &bucket = rawHistories[s].buckets[i];
            bucket.startUtc = histories[s].buckets[i].startUtc;
            bucket.count = histories[s].buckets[i].count;
            bucket.sum = bucket.minimum = bucket.maximum = bucket.latest =
                histories[s].buckets[i].latest;
        }
    }
    ++status.revision;
    refreshHistoryChartData(page, rawHistories, status, false);
    check(draws == 3, "legacy numeric histories still render with unchanged point values");
    static const HistoryChartSeries dailySeries[] = {
        {"pages.om-temp", HistoryAggregation::Minimum, CYD_CYAN, "Low"},
        {"pages.om-temp", HistoryAggregation::Maximum, CYD_CYAN, "High"},
    };
    const HistoryChartPage dailyPage = {
        "Daily Temps - 30 Days", dailySeries, 2, 30 * 86400, 86400, "30 days", " F",
        1, 10, false, scale, HistoryChartStyle::DailyFloatingBars,
    };
    for (size_t s = 0; s < 2; ++s)
    {
        histories[s].startUtc = 1791244800;
        histories[s].endUtc = histories[s].startUtc + 30 * 86400;
        histories[s].bucketSeconds = 86400;
        histories[s].aggregation = dailySeries[s].aggregation;
        histories[s].sampleCount = 30;
        histories[s].buckets.resize(30);
        for (size_t i = 0; i < 30; ++i)
        {
            auto &bucket = histories[s].buckets[i];
            bucket.startUtc = histories[s].startUtc + i * 86400;
            bucket.count = 1;
            bucket.latest = static_cast<double>(i) + (s ? 10 : -5);
        }
        histories[s].minimum = histories[s].buckets.front().latest;
        histories[s].maximum = histories[s].buckets.back().latest;
    }
    drawHistoryChartFrame(dailyPage);
    ++status.revision;
    refreshHistoryChartData(dailyPage, histories, status, false);
    check(draws == 4 &&
          std::find(graphics.printed.begin(), graphics.printed.end(), "30/30 days") !=
              graphics.printed.end(),
          "30 paired daily bars render with a 30/30 footer");
    const HistoryChartPage forecastPage = {
        "Daily Temps - 28 Days", dailySeries, 2, 28 * 86400, 86400, "28 days", " F",
        1, 10, false, scale, HistoryChartStyle::DailyForecastFloatingBars,
    };
    check(!parseDailyForecastHistory(DAILY_TEMPERATURE_SNAPSHOT, DAILY_TEMPERATURE_FIELDS,
                                    2, histories), "parse daily forecast renderer fixture");
    setTestTimezone("UTC12");
    drawHistoryChartFrame(forecastPage);
    ++status.revision;
    failAllocation = true;
    refreshHistoryChartData(forecastPage, histories, status, false);
    check(dailyForecastDraws == 0 && graphics.printed.back() == "Not enough memory for chart",
          "daily forecast allocation failure is visible");
    refreshHistoryChartData(forecastPage, histories, status, false);
    check(dailyForecastDraws == 1 &&
          std::find(graphics.printed.begin(), graphics.printed.end(), "28/28 days") !=
              graphics.printed.end() &&
          std::find(graphics.printed.begin(), graphics.printed.end(), "10/28") !=
              graphics.printed.end() &&
          std::find(graphics.printed.begin(), graphics.printed.end(), "11/24") !=
              graphics.printed.end(),
          "daily forecast retries and labels the actual date window and paired-day count");
    refreshHistoryChartData(forecastPage, histories, status, false);
    check(dailyForecastDraws == 1, "unchanged daily forecast is not redrawn");
    refreshHistoryChartData(forecastPage, histories, status, true);
    check(dailyForecastDraws == 2, "stale daily forecast redraws retained bars");
    const double previousDailyDivider = dailyDivider;
    setTestTimezone("UTC-12");
    refreshHistoryChartData(forecastPage, histories, status, true);
    check(dailyForecastDraws == 3 && dailyDivider == previousDailyDivider + 86400,
          "daily divider follows local midnight without a new feed revision");
    refreshHistoryChartData(forecastPage, histories, status, true);
    check(dailyForecastDraws == 3, "new daily divider date is cached");
    check(!parseDailyForecastHistory("[]", DAILY_TEMPERATURE_FIELDS, 2, histories),
          "parse empty daily forecast");
    ++status.revision;
    refreshHistoryChartData(forecastPage, histories, status, false);
    check(dailyForecastDraws == 3 && graphics.printed.back() == "No paired daily readings",
          "empty daily forecast shows no fabricated bars");
    static const HistoryChartSeries trendSeries[] = {
        {"pages.baro-trend", HistoryAggregation::Minimum, CYD_GREEN, "Low"},
        {"pages.baro-trend", HistoryAggregation::Maximum, CYD_GREEN, "High"},
        {"pages.baro-trend", HistoryAggregation::Average, COLOR_TEXT, "Average"},
    };
    const HistoryChartPage trendPage = {
        "Barometer Trend", trendSeries, 3, 72 * 3600, 10800, "3 days", " hPa",
        1, 1, false, scale, HistoryChartStyle::PressureTrendBars,
    };
    static SnapshotHistoryResult trends[3];
    for (size_t s = 0; s < 3; ++s)
    {
        auto &history = trends[s];
        history.startUtc = 1794092400;
        history.endUtc = history.startUtc + 25 * 10800;
        history.bucketSeconds = 10800;
        history.sampleCount = 25;
        history.aggregation = trendSeries[s].aggregation;
        history.buckets.resize(25);
        for (size_t i = 0; i < 25; ++i)
        {
            history.buckets[i].startUtc = history.startUtc + i * 10800;
            history.buckets[i].count = 1;
            history.buckets[i].latest = (s == 0 ? 1018 : s == 1 ? 1020 : 1018.25) + i * 0.1;
        }
        history.minimum = history.buckets.front().latest;
        history.maximum = history.buckets.back().latest;
    }
    drawHistoryChartFrame(trendPage);
    ++status.revision;
    failAllocation = true;
    refreshHistoryChartData(trendPage, trends, status, false);
    check(trendDraws == 0 && graphics.printed.back() == "Not enough memory for chart",
          "trend point allocation failure is visible");
    refreshHistoryChartData(trendPage, trends, status, false);
    check(trendDraws == 1 &&
          std::find(graphics.printed.begin(), graphics.printed.end(), "25 bars") !=
              graphics.printed.end(), "trend retries and shows bucket count, not series point count");
    refreshHistoryChartData(trendPage, trends, status, false);
    check(trendDraws == 1, "unchanged trend revision does not redraw");
    refreshHistoryChartData(trendPage, trends, status, true);
    check(trendDraws == 2, "stale trend redraws all three series");
    static const HistoryChartSeries cloudSeries[] = {
        {"pages.om-sun", HistoryAggregation::Average, CYD_YELLOW, "Sunshine"},
        {"pages.om-sun", HistoryAggregation::Average, CYD_RED, "Cloud cover"},
    };
    const HistoryChartPage cloudPage = {
        "Clouds", cloudSeries, 2, 28 * 86400, 86400, "28 days", "%",
        0, 0, true, scale, HistoryChartStyle::CloudBarsLine,
    };
    check(!parseCloudsHistory(CLOUDS_SNAPSHOT, trends), "parse cloud renderer fixture");
    setTestTimezone("UTC12");
    drawHistoryChartFrame(cloudPage);
    ++status.revision;
    failAllocation = true;
    refreshHistoryChartData(cloudPage, trends, status, false);
    check(cloudDraws == 0 && graphics.printed.back() == "Not enough memory for chart",
          "cloud point allocation failure is visible");
    refreshHistoryChartData(cloudPage, trends, status, false);
    check(cloudDraws == 1 &&
          std::find(graphics.printed.begin(), graphics.printed.end(), "28 days") !=
              graphics.printed.end() &&
          std::find(graphics.printed.begin(), graphics.printed.end(), "10/28") !=
              graphics.printed.end() &&
          std::find(graphics.printed.begin(), graphics.printed.end(), "11/24") !=
              graphics.printed.end(), "cloud renderer retries and labels the actual date window");
    refreshHistoryChartData(cloudPage, trends, status, false);
    check(cloudDraws == 1, "unchanged Clouds does not redraw");
    refreshHistoryChartData(cloudPage, trends, status, true);
    check(cloudDraws == 2, "stale Clouds redraws its retained data");
    const double previousDivider = cloudDivider;
    setTestTimezone("UTC-12");
    refreshHistoryChartData(cloudPage, trends, status, true);
    check(cloudDraws == 3 && cloudDivider == previousDivider + 86400,
          "local date rollover moves divider without a feed revision or stale-state change");
    refreshHistoryChartData(cloudPage, trends, status, true);
    check(cloudDraws == 3, "new local date is cached after successful redraw");
    check(!parseCloudsHistory("[]", trends), "parse empty clouds fixture");
    ++status.revision;
    refreshHistoryChartData(cloudPage, trends, status, false);
    check(cloudDraws == 3 && graphics.printed.back() == "No cloud/sunshine readings",
          "empty Clouds shows an explicit message without claiming a past-only window");
    std::printf("PASS: %zu history renderer memory checks\n", checks);
    return 0;
}
