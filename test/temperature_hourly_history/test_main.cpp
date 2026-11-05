#include "app.h"
#include <HTTPClient.h>

#define private public
#include "screens/history_chart_page.h"
#undef private
#include "screens/temperature_hourly_history.h"

#include <cstdlib>
#include <string>

TestSerial Serial;
TestWiFi WiFi;
uint32_t testMillis = 1000;
std::deque<TestHttpResponse> testResponses;
std::vector<std::string> testUrls;
std::function<void()> testOnGet;
AdafruitIOManager adafruitIOManager;
AppState state = {SCREEN_TEMPERATURES};
static size_t checks = 0;
static size_t snapshotFrames = 0;
static size_t originalCharts = 0;
static NumericHistoryResult displayedHistory;
static NumericHistoryStatus displayedStatus;
static bool displayedStale = false;

static void check(bool condition, const char *message)
{
    ++checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static void checkNear(double value, double expected, const char *message)
{
    if (std::fabs(value - expected) > 1e-5)
        std::fprintf(stderr, "Expected %.12g, got %.12g\n", expected, value);
    check(std::isfinite(value) && std::fabs(value - expected) <= 1e-5, message);
}

void drawCurrentScreen() { drawTemperatureHistoryScreen(); }
void drawNav2(const char *) {}
bool navPressed1(int x, int) { return x == 1; }
bool navPressed2(int x, int) { return x == 2; }
void enterTemperaturesScreen() { state.currentScreen = SCREEN_TEMPERATURES; }
void enterTemperatureDailyScreen() { state.currentScreen = SCREEN_TEMPERATURE_DAILY; }
void drawHistoryChartPage(const HistoryChartPage &page)
{
    ++originalCharts;
    (void)page;
    check(false, "temperature history must not request raw history");
}
void refreshHistoryChartPage(const HistoryChartPage &) {}
void drawHistoryChartFrame(const HistoryChartPage &page)
{
    ++snapshotFrames;
    check(std::strcmp(page.title, "Last 72 Hours") == 0 &&
          page.windowSeconds == 72 * 3600 && page.bucketSeconds == 3600,
          "temperature chart requests a 72-hour frame with hourly spacing");
}
void refreshHistoryChartData(const HistoryChartPage &,
                            const NumericHistoryResult histories[],
                            const NumericHistoryStatus &status, bool stale)
{
    displayedHistory = histories[0];
    displayedStatus = status;
    displayedStale = stale;
}

static int64_t hour(const char *text)
{
    int64_t timestamp = 0;
    check(parseTemperatureHour(text, timestamp), "parse valid local calendar hour");
    return timestamp;
}

static std::string suppliedSnapshot()
{
    const double temperatures[] = {
        69.0,68.7,67.0,66.3,64.2,62.6,61.8,62.2,61.5,61.8,61.2,62.4,
        61.9,63.0,65.6,69.4,72.0,73.6,74.5,74.2,74.3,72.9,71.4,69.5,
        67.6,66.9,65.8,64.4,63.3,62.8,61.8,60.9,60.6,60.4,60.2,59.9,
        59.8,59.7,60.5,61.2,62.3,63.5,63.4,63.6,64.8,63.4,62.1,62.8,
        62.1,61.8,62.1,62.3,62.3,62.6,62.7,63.0,63.2,63.5,63.8,63.5,
        63.9,64.8,65.9,67.2,69.1,71.7,72.4,73.1,73.1,71.7,69.3,68.1,
        66.8,66.0,66.8,66.4,66.7,67.3,67.3,67.0,66.6,66.5,66.0,65.4,
        66.1,68.2,70.0,71.8,73.2,73.7,74.6,74.7,73.7,72.9,71.8,70.4,
        67.9,67.1,67.6,66.9,67.0,66.8,66.1,66.5,65.7,65.1,63.8,62.3,
        61.6,62.0,62.6,63.6
    };
    int64_t first;
    check(parseHistoryTimestamp("2026-10-30T19:00:00Z", first),
          "prepare supplied local-calendar fixture");
    std::string json = "[";
    for (size_t i = 0; i < sizeof(temperatures) / sizeof(temperatures[0]); ++i)
    {
        const time_t calendar = static_cast<time_t>(first / 1000 + i * 3600);
        char text[16];
        std::strftime(text, sizeof(text), "%Y%m%d%H", std::gmtime(&calendar));
        char entry[96];
        std::snprintf(entry, sizeof(entry), "%s{\"ts\":\"%s\",\"ftemp\":%.1f}",
                      i ? "," : "", text, temperatures[i]);
        json += entry;
    }
    json += "]";
    return json;
}

int main()
{
#ifdef _WIN32
    _putenv_s("TZ", "CST6CDT");
    _tzset();
#else
    setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
    tzset();
#endif
    const std::string snapshot = suppliedSnapshot();
    NumericHistoryResult result;
    check(!parseTemperatureHourlyHistory(snapshot.c_str(), result),
          "parse all 112 supplied hourly readings");
    check(result.buckets.size() == 72 && result.sampleCount == 72 &&
          result.endUtc - result.startUtc == 72 * 3600 &&
          result.startUtc == hour("2026110111") &&
          result.endUtc == hour("2026110411"),
          "select exactly the newest 72 elapsed hours, including latest hour");
    checkNear(result.minimum, 61.6, "minimum excludes earlier out-of-window values");
    checkNear(result.maximum, 74.7, "maximum uses only visible readings");
    checkNear(result.buckets.front().value(HistoryAggregation::Average), 62.3,
              "first visible point uses its supplied hourly average");
    checkNear(result.buckets.back().value(HistoryAggregation::Average), 63.6,
              "last visible point uses its supplied hourly average");
    check(!parseTemperatureHourlyHistory(
        "[{\"ts\":\"2026110410\",\"ftemp\":-5.5},"
        "{\"ts\":\"2026110408\",\"ftemp\":0}]", result) &&
          result.sampleCount == 2 && result.minimum == -5.5 &&
          result.maximum == 0 && result.buckets[70].count == 0 &&
          std::isnan(result.buckets[70].value(HistoryAggregation::Average)),
          "sort input, retain negative/zero values, and leave missing hours as gaps");
    check(hour("2026110102") - hour("2026110101") == 3600 &&
          hour("2026110101") - hour("2026110100") == 7200,
          "fall-back's ambiguous 01 selects standard time and preserves elapsed spacing");
    check(hour("2026030803") - hour("2026030801") == 3600,
          "spring-forward local hours preserve real one-hour spacing");
    int64_t ignored;
    check(!parseTemperatureHour("2026030802", ignored),
          "reject nonexistent spring-forward hour");
    check(parseTemperatureHour("2028022900", ignored),
          "accept leap day");
    for (const char *invalid : {
        "", "{", "{}", "null", "[1]", "[{}]",
        "[{\"ts\":2026110410,\"ftemp\":1}]",
        "[{\"ts\":\"2026110410\"}]",
        "[{\"ts\":\"2026110410\",\"ftemp\":\"1\"}]",
        "[{\"ts\":\"2026110410\",\"ftemp\":null}]",
        "[{\"ts\":\"2026110410\",\"ftemp\":true}]",
        "[{\"ts\":\"2026110410\",\"ftemp\":1e1000}]",
        "[{\"ts\":\"2026022900\",\"ftemp\":1}]",
        "[{\"ts\":\"2026130100\",\"ftemp\":1}]",
        "[{\"ts\":\"2026110424\",\"ftemp\":1}]",
        "[{\"ts\":\"20261104xx\",\"ftemp\":1}]",
        "[{\"ts\":\"20261104100\",\"ftemp\":1}]",
        "[{\"ts\":\"2026110410\",\"ftemp\":1},"
         "{\"ts\":\"2026110410\",\"ftemp\":2}]",
        "[{\"ts\":\"2026110410\",\"ftemp\":1},{}]"
    })
    {
        const int64_t previousEnd = result.endUtc;
        check(parseTemperatureHourlyHistory(invalid, result) != nullptr,
              "malformed snapshot returns an explicit error");
        check(result.endUtc == previousEnd && result.sampleCount == 2 &&
              result.minimum == -5.5, "invalid snapshot cannot partially replace chart data");
    }
    check(!parseTemperatureHourlyHistory("[]", result) &&
          result.sampleCount == 0 && result.buckets.empty(),
          "empty snapshot is successful empty data");
    check(!parseTemperatureHourlyHistory(
        "[{\"ts\":\"2026110410\",\"ftemp\":1.25}]", result) &&
          result.sampleCount == 1 && result.buckets.size() == 72 &&
          result.minimum == 1.25 && result.maximum == 1.25,
          "single reading recovers after empty or malformed snapshots");

    check(adafruitIOManager.begin(), "start fake manager");
    NumericHistoryRequest request;
    check(adafruitIOManager.requestNumericHistory("feed-avg", request),
          "prepare pre-existing raw history subscription");
    enterTemperatureHistoryScreen();
    check(state.currentScreen == SCREEN_TEMPERATURE_HISTORY &&
          snapshotFrames == 1 && originalCharts == 0 &&
          adafruitIOManager._numericSeriesCount == 0,
          "temperature history opens only snapshots and cancels raw retrieval");
    const int feed = adafruitIOManager.findFeed("pages.temp-line");
    check(feed >= 0, "hourly snapshot feed is registered");
    for (size_t i = 0; i < AIO_FEED_COUNT; ++i)
        check(adafruitIOManager._feedCache[i].active ==
                  (static_cast<int>(i) == feed),
              "temperature history subscribes only to pages.temp-line");
    check(AIO_FEEDS[feed].mayExceed45Chars &&
          AIO_FEEDS[feed].refreshIntervalMinutes == 1,
          "hourly feed supports long snapshots and minute polling");
    check(!displayedStatus.hasData &&
          displayedStatus.state == NumericHistoryState::Loading,
          "missing snapshot displays loading rather than an empty success");

    auto &cache = adafruitIOManager._feedCache[feed];
    cache.hasValue = true;
    cache.value = snapshot.c_str();
    cache.updatedAt = testMillis;
    refreshTemperatureHistoryResults();
    check(displayedStatus.hasData && displayedStatus.state == NumericHistoryState::Ready &&
          displayedHistory.sampleCount == 72,
          "snapshot data reaches shared renderer without fetching raw records");
    const uint32_t revision = displayedStatus.revision;
    refreshTemperatureHistoryResults();
    check(displayedStatus.revision == revision,
          "unchanged payload does not rebuild history");
    cache.value = "bad JSON";
    refreshTemperatureHistoryResults();
    check(displayedStatus.hasData && displayedStatus.state == NumericHistoryState::Failed &&
          displayedHistory.sampleCount == 72 && displayedStale,
          "malformed update retains valid chart with explicit stale failure");
    cache.value = "[]";
    refreshTemperatureHistoryResults();
    check(displayedStatus.hasData && displayedStatus.state == NumericHistoryState::Ready &&
          displayedHistory.sampleCount == 0,
          "empty update replaces old readings with a successful empty window");
    const time_t now = std::time(nullptr);
    char fresh[96];
    char currentHour[16];
    std::strftime(currentHour, sizeof(currentHour), "%Y%m%d%H", std::localtime(&now));
    std::snprintf(fresh, sizeof(fresh), "[{\"ts\":\"%s\",\"ftemp\":60}]", currentHour);
    cache.value = fresh;
    cache.updatedAt = testMillis;
    refreshTemperatureHistoryResults();
    check(!displayedStale, "current hourly snapshot and fresh cache are not stale");
    const uint32_t freshRevision = displayedStatus.revision;
    testMillis += 120000;
    refreshTemperatureHistoryResults();
    check(!displayedStale, "cache remains fresh at exactly two minutes");
    ++testMillis;
    refreshTemperatureHistoryResults();
    check(displayedStale, "old feed cache is marked stale");
    cache.updatedAt = testMillis;
    refreshTemperatureHistoryResults();
    check(!displayedStale && displayedStatus.revision == freshRevision,
          "successful unchanged feed poll clears stale without rebuilding data");
    cache.value = snapshot.c_str();
    refreshTemperatureHistoryResults();
    check(adafruitIOManager._numericSeriesCount == 0 && testUrls.empty(),
          "snapshot refreshes never issue raw numeric-history requests");
    handleTemperatureHistoryTouch(1, 0);
    check(state.currentScreen == SCREEN_TEMPERATURES,
          "temperature history Back returns to Temperatures");
    enterTemperatureHistoryScreen();
    check(snapshotFrames == 2 && originalCharts == 0,
          "reopening temperature history still uses snapshots");
    handleTemperatureHistoryTouch(1, 0);
    check(state.currentScreen == SCREEN_TEMPERATURES,
          "history Back returns to Temperatures");
    enterTemperatureHistoryScreen();
    handleTemperatureHistoryTouch(2, 0);
    check(state.currentScreen == SCREEN_TEMPERATURE_DAILY,
          "temperature history retains Daily navigation");
    std::printf("PASS: %zu hourly temperature history checks\n", checks);
    return 0;
}
