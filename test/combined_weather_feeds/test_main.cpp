#include "app.h"
#include <HTTPClient.h>

#define private public
#include "screens/snapshot_history_page.h"
#undef private
#include "screens/text_table_page.h"
#include "screens/weather_page_values.h"
#include "screens/temperature_page_values.h"
#include "screens/precipitation_page_values.h"
#include "screens/precipitation_daily_history.h"
#include "screens/barometer_trend_history.h"
#include "screens/clouds_history.h"
#include "clouds_fixture.h"
#include "temperature_daily_fixture.h"
#include "wind_daily_fixture.h"

#include <cstdlib>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <string>

TestSerial Serial;
TestWiFi WiFi;
uint32_t testMillis = 1000;
std::deque<TestHttpResponse> testResponses;
std::vector<std::string> testUrls;
std::function<void()> testOnGet;
AdafruitIOManager adafruitIOManager;
AppState state = {SCREEN_DETAILS};
static TestGraphics graphics;
TestGraphics *gfx = &graphics;
static size_t checks = 0;
static size_t failNothrowAllocation = 0;

void *operator new[](size_t size, const std::nothrow_t &) noexcept
{
    if (failNothrowAllocation && --failNothrowAllocation == 0)
        return nullptr;
    try { return ::operator new[](size); }
    catch (const std::bad_alloc &) { return nullptr; }
}

class BudgetJsonAllocator : public ArduinoJson::Allocator
{
public:
    size_t limit = 8192;
    size_t live = 0;
    size_t peak = 0;
    size_t failures = 0;
    void *allocate(size_t size) override
    {
        if (live + size > limit)
        {
            ++failures;
            return nullptr;
        }
        void *pointer = std::malloc(size);
        if (pointer)
        {
            sizes[pointer] = size;
            live += size;
            peak = std::max(peak, live);
        }
        return pointer;
    }
    void deallocate(void *pointer) override
    {
        if (pointer)
        {
            live -= sizes.at(pointer);
            sizes.erase(pointer);
            std::free(pointer);
        }
    }
    void *reallocate(void *pointer, size_t size) override
    {
        const size_t previous = pointer ? sizes.at(pointer) : 0;
        if (live - previous + size > limit)
        {
            ++failures;
            return nullptr;
        }
        void *replacement = std::realloc(pointer, size);
        if (replacement)
        {
            sizes.erase(pointer);
            sizes[replacement] = size;
            live = live - previous + size;
            peak = std::max(peak, live);
        }
        return replacement;
    }

private:
    std::map<void *, size_t> sizes;
};

static SnapshotHistoryResult displayed[3];
static NumericHistoryStatus displayedStatus;
static bool displayedStale = false;
static uint32_t displayedWindow = 0;
static uint32_t displayedBucket = 0;
static std::vector<std::string> tableValues;
static std::string navLabels[4];

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
    const double tolerance = std::max(1e-6,
        std::fabs(expected) * std::numeric_limits<float>::epsilon());
    if (std::fabs(value - expected) > tolerance)
        std::fprintf(stderr, "Expected %.12g, got %.12g\n", expected, value);
    check(std::isfinite(value) && std::fabs(value - expected) <= tolerance, message);
}

void clearScreen(uint16_t) { graphics.printed.clear(); }
void titleBar(const char *) {}
void setDefaultFont() {}
void drawNav1(const char *text) { navLabels[0] = text; }
void drawNav2(const char *text) { navLabels[1] = text; }
void drawNav3(const char *text) { navLabels[2] = text; }
void drawNav4(const char *text) { navLabels[3] = text; }
bool navPressed1(int x, int) { return x == 1; }
bool navPressed2(int x, int) { return x == 2; }
bool navPressed3(int x, int) { return x == 3; }
void enterDetailsScreen() { state.currentScreen = SCREEN_DETAILS; }
void enterWindDirectionScreen() { state.currentScreen = SCREEN_WIND_DIRECTION; }

void drawCurrentScreen()
{
    switch (state.currentScreen)
    {
    case SCREEN_TEMPERATURES: drawTemperaturesScreen(); break;
    case SCREEN_TEMPERATURE_HISTORY: drawTemperatureHistoryScreen(); break;
    case SCREEN_TEMPERATURE_DAILY: drawTemperatureDailyScreen(); break;
    case SCREEN_PRECIPITATION: drawPrecipitationScreen(); break;
    case SCREEN_PRECIPITATION_DAILY: drawPrecipitationDailyScreen(); break;
    case SCREEN_PRECIPITATION_HISTORY: drawPrecipitationHistoryScreen(); break;
    case SCREEN_WIND: drawWindScreen(); break;
    case SCREEN_WIND_HISTORY: drawWindHistoryScreen(); break;
    case SCREEN_WIND_DAILY: drawWindDailyScreen(); break;
    case SCREEN_BAROMETER: drawBarometerScreen(); break;
    case SCREEN_BAROMETER_HISTORY: drawBarometerHistoryScreen(); break;
    case SCREEN_BAROMETER_TREND: drawBarometerTrendScreen(); break;
    case SCREEN_CLOUDS: drawCloudsScreen(); break;
    case SCREEN_ASTRONOMICAL: drawAstronomicalScreen(); break;
    default: break;
    }
}

bool drawTextTablePage(const TextTablePage &page)
{
    tableValues.clear();
    for (size_t i = 0; i < page.rowCount; ++i)
        tableValues.emplace_back(page.rows[i].value ? page.rows[i].value : "");
    return true;
}
bool refreshTextTablePageRows(const TextTablePage &page) { return drawTextTablePage(page); }
bool handleTextTablePageTouch(const TextTablePage &page, int x, int)
{
    if (x == 1 && page.backAction) page.backAction();
    else if (x == 2 && page.button2.action) page.button2.action();
    else if (x == 3 && page.button3.action) page.button3.action();
    return true;
}
void drawHistoryChartPage(const HistoryChartPage &) { check(false, "no raw history charts"); }
void refreshHistoryChartPage(const HistoryChartPage &) { check(false, "no raw history refresh"); }
void drawHistoryChartFrame(const HistoryChartPage &page)
{
    displayedWindow = page.windowSeconds;
    displayedBucket = page.bucketSeconds;
}
void refreshHistoryChartData(const HistoryChartPage &page,
                            const SnapshotHistoryResult histories[],
                            const NumericHistoryStatus &status, bool stale)
{
    for (size_t i = 0; i < page.seriesCount; ++i)
        displayed[i] = histories[i];
    displayedStatus = status;
    displayedStale = stale;
}

static void assertOnlyFeed(const char *key, uint16_t intervalMinutes = 1)
{
    const int feed = adafruitIOManager.findFeed(key);
    check(feed >= 0, "combined feed is registered");
    for (size_t i = 0; i < AIO_FEED_COUNT; ++i)
        check(adafruitIOManager._feedCache[i].active == (static_cast<int>(i) == feed),
              "visible page activates exactly its combined feed");
    check(AIO_FEEDS[feed].mayExceed45Chars &&
          AIO_FEEDS[feed].refreshIntervalMinutes == intervalMinutes,
          "combined feeds preserve long JSON and use their configured polling interval");
}

static void cache(const char *key, const char *payload)
{
    const int feed = adafruitIOManager.findFeed(key);
    check(feed >= 0, "cache fixture feed exists");
    auto &entry = adafruitIOManager._feedCache[feed];
    entry.hasValue = true;
    entry.value = payload;
    ++entry.revision;
    entry.updatedAt = testMillis;
}

static const char *windPage =
    "[\"SSW\",\"4\",\"7\",\"SSW\",\"4\",\"7\",\"SW\",\"4\",\"6\","
    "\"SSW\",\"3\",\"5\",\"NW\",\"3\",\"5\",\"N\",\"10\",\"17\"]";
static const char *baroPage =
    "[\"1013.03\",\"falling\",\"1013.02\",\"falling\",\"1012.65\",\"falling\","
    "\"1014.04\",\"rising\",\"1015.74\",\"steady\",\"1019.44\",\"steady\"]";
static const char *astroPage =
    "[\"6:23 AM\",\"5:01 PM\",\"3:49 AM\",\"3:19 PM\",\"Waning Crescent\"]";
static const char *parsePayload(const std::string &key, const char *payload)
{
    TemperaturePageValues temperatures;
    PrecipitationPageValues precipitation;
    WindPageValues wind;
    BarometerPageValues barometer;
    AstronomicalPageValues astronomical;
    int daylight = 0;
    if (key == "pages.p01-temp") return parseTemperaturePageValues(payload, temperatures);
    if (key == "pages.p02-precipitation") return parsePrecipitationPageValues(payload, precipitation);
    if (key == "pages.p03-wind") return parseWindPageValues(payload, wind);
    if (key == "pages.p04-baro") return parseBarometerPageValues(payload, barometer);
    if (key == "pages.p05-astro") return parseAstronomicalPageValues(payload, astronomical, daylight);
    SnapshotHistoryResult histories[3];
    if (key == "pages.baro-trend")
        return parseBarometerTrendHistory(payload, histories);
    if (key == "pages.om-temp")
        return parseDailyForecastHistory(payload, DAILY_TEMPERATURE_FIELDS, 2, histories);
    if (key == "pages.om-sun")
        return parseCloudsHistory(payload, histories);
    if (key == "pages.om-precip")
        return parsePrecipitationDailyHistory(payload, histories);
    if (key == "pages.om-wind")
        return parseDailyForecastHistory(payload, DAILY_WIND_FIELDS, 2, histories);
    const SnapshotHistoryField fields[] = {
        {key == "pages.temp-line" ? "ftemp" : key == "pages.baro-line" ? "baro" :
         key == "pages.prcp-line" ? "prcp" : "speed", HistoryAggregation::Average},
        {"gust", HistoryAggregation::Maximum},
    };
    return parseWeatherSnapshotHistory(payload, fields, key == "pages.wind-line" ? 2 : 1,
                                       key == "pages.prcp-line", histories);
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    _putenv_s("TZ", "CST6CDT");
    _tzset();
#else
    setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
    tzset();
#endif
    if (argc == 2)
    {
        const std::string payload((std::istreambuf_iterator<char>(std::cin)),
                                  std::istreambuf_iterator<char>());
        const char *error = parsePayload(argv[1], payload.c_str());
        if (error)
        {
            std::fprintf(stderr, "FAIL: %s: %s\n", argv[1], error);
            return 1;
        }
        std::printf("PASS: supplied %s payload\n", argv[1]);
        return 0;
    }

    WindPageValues wind;
    BarometerPageValues barometer;
    AstronomicalPageValues astronomical;
    int daylight = -1;
    check(!parseWindPageValues(windPage, wind) &&
          std::strcmp(wind[0].data(), "SSW") == 0 &&
          std::strcmp(wind[15].data(), "N") == 0 &&
          std::strcmp(wind[17].data(), "17") == 0, "wind row-major Current through 24 Hours");
    check(!parseBarometerPageValues(baroPage, barometer) &&
          std::strcmp(barometer[0].data(), "1013.03") == 0 &&
          std::strcmp(barometer[10].data(), "1019.44") == 0, "barometer pair ordering");
    check(!parseAstronomicalPageValues(astroPage, astronomical, daylight) &&
          daylight == 638, "supplied sunrise/sunset calculate 10:38 daylight");
    int minutes;
    check(parseAstronomicalClock("12:00 AM", minutes) && minutes == 0 &&
          parseAstronomicalClock("12:00 PM", minutes) && minutes == 720,
          "12 AM/PM boundary conversion");
    check(!parseAstronomicalPageValues(
        "[\"11:30 PM\",\"1:00 AM\",\"\",\"\",\"Full Moon\"]", astronomical, daylight) &&
          daylight == 90, "overnight clock duration and absent moon events");
    const AstronomicalPageValues previousAstro = astronomical;
    for (const char *invalid : {"{}", "[]", "[1,2,3,4,5]",
        "[\"6:23 AM\",\"25:00 PM\",\"\",\"\",\"Full Moon\"]",
        "[\"0:23 AM\",\"5:01 PM\",\"\",\"\",\"Full Moon\"]"})
    {
        check(parseAstronomicalPageValues(invalid, astronomical, daylight) != nullptr &&
              astronomical == previousAstro && daylight == 90, "invalid Astro is rejected atomically");
    }
    const WindPageValues previousWind = wind;
    check(parseWindPageValues("[]", wind) && wind == previousWind, "invalid wind length retains values");
    std::string invalidWind(windPage);
    invalidWind.replace(invalidWind.find("\"4\""), 3, "\"NaN\"");
    check(parseWindPageValues(invalidWind.c_str(), wind) && wind == previousWind,
          "nonfinite numeric strings cannot replace wind rows");
    check(parseBarometerPageValues("[true,\"steady\"]", barometer),
          "invalid barometer types/length rejected");

    const SnapshotHistoryField windFields[] = {
        {"speed", HistoryAggregation::Average},
        {"gust", HistoryAggregation::Maximum},
    };
    const SnapshotHistoryField baroFields[] = {{"baro", HistoryAggregation::Average}};
    const SnapshotHistoryField prcpFields[] = {{"prcp", HistoryAggregation::Maximum}};
    SnapshotHistoryResult histories[2];
    check(!parseWeatherSnapshotHistory(
        "[{\"ts\":\"2026110500\",\"speed\":0,\"gust\":null},"
        "{\"ts\":\"2026110501\",\"speed\":4,\"gust\":7}]", windFields, 2, false, histories) &&
          histories[0].buckets.size() == 2 && histories[0].sampleCount == 2 &&
          histories[1].sampleCount == 1 && histories[0].minimum == 0 &&
          histories[1].maximum == 7 &&
          histories[0].buckets[0].latest == 0 && histories[1].buckets[0].count == 0 &&
          histories[0].startUtc == histories[1].startUtc,
          "wind preserves supplied ordering and leading null without filling or aggregation");
    const int64_t previousEnd = histories[0].endUtc;
    for (const char *invalid : {
        "[{\"ts\":\"2026110502\",\"speed\":4}]",
        "[{\"ts\":\"2026110502\",\"speed\":4,\"gust\":\"7\"}]",
        "[{\"ts\":\"2026110502\",\"speed\":4,\"gust\":7},{}]"})
        check(parseWeatherSnapshotHistory(invalid, windFields, 2, false, histories) &&
              histories[0].endUtc == previousEnd && histories[1].sampleCount == 1,
              "wind snapshots reject missing/malformed fields atomically");
    check(!parseWeatherSnapshotHistory(
        "[{\"ts\":\"2026110100\",\"baro\":9999},"
        "{\"ts\":\"2026110500\",\"baro\":1013.09}]", baroFields, 1, false, histories) &&
          histories[0].sampleCount == 1 &&
          histories[0].buckets.size() == 1, "barometer trims history and extrema to 72 hours");
    checkNear(histories[0].maximum, 1013.09, "barometer visible maximum excludes older records");
    check(!parseWeatherSnapshotHistory(
        "[{\"ts\":\"20261001\",\"prcp\":100},"
        "{\"ts\":\"20261006\",\"prcp\":0.25},"
        "{\"ts\":\"20261104\",\"prcp\":0.12}]", prcpFields, 1, true, histories) &&
          histories[0].buckets.size() == 2 && histories[0].sampleCount == 2 &&
          histories[0].endUtc - histories[0].startUtc == 30 * 86400 &&
          histories[0].bucketSeconds == 86400 && histories[0].maximum == 0.25 &&
          histories[0].buckets.front().latest == 0.25 &&
          std::fabs(histories[0].buckets.back().latest - 0.12) < 1e-6,
          "precipitation selects exact 30 local dates");
    check(!parseWeatherSnapshotHistory("[]", prcpFields, 1, true, histories) &&
          histories[0].buckets.empty() && histories[0].sampleCount == 0,
          "empty precipitation does not fabricate daily readings");
    int64_t timestamp;
    check(!parseSnapshotDate("20260229", timestamp) &&
          parseSnapshotDate("20280229", timestamp), "strict leap-day date validation");
    check(parseWeatherSnapshotHistory(
        "[{\"ts\":\"20260229\",\"prcp\":1}]", prcpFields, 1, true, histories),
          "invalid precipitation dates are rejected");

    std::string longWind = "[";
    int64_t firstHour;
    check(parseHistoryTimestamp("2026-09-01T00:00:00Z", firstHour), "prepare long history fixture");
    for (size_t i = 0; i < 1000; ++i)
    {
        const time_t timestamp = static_cast<time_t>(firstHour / 1000 + i * 3600);
        char hour[16];
        std::strftime(hour, sizeof(hour), "%Y%m%d%H", std::gmtime(&timestamp));
        longWind += std::string(i ? "," : "") + "{\"ts\":\"" + hour +
            "\",\"speed\":" + std::to_string(i) + ",\"gust\":" + std::to_string(i + 3) + "}";
    }
    longWind += "]";
    BudgetJsonAllocator budget;
    check(!parseWeatherSnapshotHistory(longWind.c_str(), windFields, 2, false, histories, &budget) &&
          histories[0].sampleCount == 72 && histories[1].sampleCount == 72 &&
          histories[0].buckets.front().latest == 928 && histories[0].buckets.back().latest == 999 &&
          histories[1].buckets.front().latest == 931 && histories[1].buckets.back().latest == 1002 &&
          budget.peak <= 8192 && budget.live == 0 && budget.failures == 0,
          "1000-hour wind snapshot parses with at most 8 KB of live JSON memory");
    std::string longPrecipitation = "[";
    for (size_t i = 0; i < 35; ++i)
    {
        const time_t timestamp = static_cast<time_t>(firstHour / 1000 + i * 86400);
        char day[16];
        std::strftime(day, sizeof(day), "%Y%m%d", std::gmtime(&timestamp));
        longPrecipitation += std::string(i ? "," : "") + "{\"ts\":\"" + day +
            "\",\"prcp\":" + std::to_string(i) + "}";
    }
    longPrecipitation += "]";
    SnapshotHistoryResult dailyHistory;
    check(!parseWeatherSnapshotHistory(longPrecipitation.c_str(), prcpFields, 1, true, &dailyHistory) &&
          dailyHistory.buckets.size() == 30 && dailyHistory.sampleCount == 30 &&
          dailyHistory.buckets.front().latest == 5 && dailyHistory.buckets.back().latest == 34 &&
          dailyHistory.minimum == 5 && dailyHistory.maximum == 34,
          "35 unique daily totals retain the newest 30 values unchanged");
    check(!parseWeatherSnapshotHistory(
        "[{\"ts\":\"2026110500\",\"baro\":null},"
        "{\"ts\":\"2026110501\",\"baro\":1013}]", baroFields, 1, false, &dailyHistory) &&
          dailyHistory.buckets.size() == 2 && dailyHistory.sampleCount == 1 &&
          dailyHistory.buckets.front().count == 0 && dailyHistory.maximum == 1013,
          "leading null barometer remains unplotted without filling");
    budget.limit = 0;
    check(parseWeatherSnapshotHistory(longWind.c_str(), windFields, 2, false, histories, &budget) &&
          histories[0].sampleCount == 72 && histories[1].sampleCount == 72 &&
          budget.live == 0, "JSON allocation failure preserves both valid series without abort");
    failNothrowAllocation = 1;
    check(parseWeatherSnapshotHistory(longWind.c_str(), windFields, 2, false, histories) &&
          histories[0].sampleCount == 72 && histories[1].sampleCount == 72,
          "staging allocation failure preserves data without throwing");
    for (const char *invalid : {
        "[{\"ts\":\"2026110500\",\"speed\":4,\"gust\":7},]",
        "[{\"ts\":\"2026110500\",\"speed\":4,\"gust\":7}] garbage",
        "[{\"ts\":\"2026110500\",\"speed\":4,\"gust\":7}",
        "[{\"ts\":\"2026110500\",\"speed\":4,\"gust\":7}"
         "{\"ts\":\"2026110501\",\"speed\":4,\"gust\":7}]"})
        check(parseWeatherSnapshotHistory(invalid, windFields, 2, false, histories) &&
              histories[0].sampleCount == 72, "streaming parser strictly validates array boundaries");

    check(adafruitIOManager.begin(), "fake manager starts");
    cache("pages.p01-temp", "[71.4,70.6,75.0,48.95,75.2,70.9]");
    enterTemperaturesScreen();
    assertOnlyFeed("pages.p01-temp");
    check(tableValues.size() == 6 && tableValues[0] == "71.4 F (21.9 C)",
          "promoted temperature page retains six rows and F/C formatting");
    handleTemperaturesTouch(3, 0);
    check(state.currentScreen == SCREEN_TEMPERATURE_HISTORY, "Temps History route");
    assertOnlyFeed("pages.temp-line");
    check(displayedWindow == 72 * 3600 && displayedBucket == 3600, "Temps 72-hour hourly frame");
    handleTemperatureHistoryTouch(1, 0);
    check(state.currentScreen == SCREEN_TEMPERATURES, "Temps history Back route");
    handleTemperaturesTouch(2, 0);
    check(state.currentScreen == SCREEN_TEMPERATURE_DAILY, "Temps Daily route preserved");
    assertOnlyFeed("pages.om-temp", 60);
    {
        const int feed = adafruitIOManager.findFeed("pages.om-temp");
        size_t historyCount = 0;
        check(!adafruitIOManager._feedCache[feed].attempted,
              "daily forecast fetch is immediately due on entry");
        for (size_t i = 0; i < AIO_FEED_COUNT; ++i)
        {
            auto &entry = adafruitIOManager._feedCache[i];
            entry.attempted = true;
            entry.failed = false;
            entry.lastAttempt = testMillis;
        }
        check(adafruitIOManager.findDueFeed(testMillis + 3599999U, historyCount) != feed,
              "daily forecast does not refresh before one hour");
        for (size_t i = 0; i < AIO_FEED_COUNT; ++i)
            if (static_cast<int>(i) != feed)
                adafruitIOManager._feedCache[i].lastAttempt = testMillis + 3600000U;
        check(adafruitIOManager.findDueFeed(testMillis + 3600000U, historyCount) == feed,
              "daily forecast refresh is due at exactly one hour");
    }
    check(displayedWindow == 28 * 86400 && displayedBucket == 86400,
          "daily temperature uses a 28-day daily frame");
    cache("pages.om-temp", DAILY_TEMPERATURE_SNAPSHOT);
    refreshTemperatureDailyResults();
    int64_t firstDay, lastDay;
    check(parseSnapshotDate("20261028", firstDay) && parseSnapshotDate("20261124", lastDay),
          "prepare supplied daily temperature endpoints");
    check(displayedStatus.state == NumericHistoryState::Ready &&
          !displayedStale &&
          displayed[0].buckets.size() == 28 && displayed[1].buckets.size() == 28 &&
          displayed[0].sampleCount == 28 && displayed[1].sampleCount == 28 &&
          displayed[0].startUtc == firstDay && displayed[0].endUtc == lastDay + 86400 &&
          displayed[0].buckets.front().startUtc == firstDay &&
          displayed[1].buckets.back().startUtc == lastDay &&
          std::fabs(displayed[0].minimum - 32.36) < 0.00001 &&
          std::fabs(displayed[1].maximum - 81.73) < 0.00001,
          "supplied snapshot preserves all past/forecast dates and paired extrema");
    for (size_t i = 0; i < 28; ++i)
    {
        check(displayed[0].buckets[i].startUtc == firstDay + static_cast<int64_t>(i) * 86400 &&
              displayed[1].buckets[i].startUtc == displayed[0].buckets[i].startUtc,
              "temperature series retain exact matching supplied dates");
        checkNear(displayed[0].buckets[i].latest, DAILY_TEMPERATURE_MIN[i], "exact daily low");
        checkNear(displayed[1].buckets[i].latest, DAILY_TEMPERATURE_MAX[i], "exact daily high");
    }
    const uint32_t dailyRevision = displayedStatus.revision;
    refreshTemperatureDailyResults();
    check(displayedStatus.revision == dailyRevision, "unchanged daily snapshot is not rebuilt");
    testMillis += 120000;
    refreshTemperatureDailyResults();
    check(!displayedStale, "daily forecasts stay fresh beyond the old two-minute timeout");
    testMillis += 7200000U - 120000U;
    refreshTemperatureDailyResults();
    check(!displayedStale, "daily forecasts remain fresh through two polling intervals");
    ++testMillis;
    refreshTemperatureDailyResults();
    check(displayedStale, "daily forecasts use receipt age rather than future endpoints");
    const char *invalidTemperatures[] = {
        "[{\"ts\":\"20261104\",\"t2m_min\":5}]",
        "[{\"ts\":\"20260230\",\"t2m_min\":0,\"t2m_max\":10}]",
        "[{\"ts\":\"20261104\",\"t2m_min\":20,\"t2m_max\":10}]",
        "[{\"ts\":\"20261104\",\"t2m_min\":0,\"t2m_max\":\"10\"}]",
        "[{\"ts\":\"20261104\",\"t2m_min\":0,\"t2m_max\":10}] trailing",
        "[{\"ts\":\"20261104\",\"t2m_min\":0,\"t2m_max\":10},"
        "{\"ts\":\"20261104\",\"t2m_min\":0,\"t2m_max\":10}]",
        "[{\"ts\":\"20261105\",\"t2m_min\":0,\"t2m_max\":10},"
        "{\"ts\":\"20261104\",\"t2m_min\":0,\"t2m_max\":10}]",
    };
    for (const char *invalid : invalidTemperatures)
    {
        cache("pages.om-temp", invalid);
        refreshTemperatureDailyResults();
        check(displayedStatus.state == NumericHistoryState::Failed && displayedStale &&
              displayed[0].sampleCount == 28 &&
              std::fabs(displayed[1].maximum - 81.73) < 0.00001,
              "invalid daily snapshot retains both previous series as stale");
    }
    BudgetJsonAllocator dailyAllocator;
    check(!parseDailyForecastHistory(DAILY_TEMPERATURE_SNAPSHOT, DAILY_TEMPERATURE_FIELDS,
                                    2, displayed, false, &dailyAllocator) &&
          dailyAllocator.peak <= 8192 && dailyAllocator.live == 0,
          "daily forecast JSON uses bounded live memory");
    dailyAllocator.limit = 1;
    check(parseDailyForecastHistory(DAILY_TEMPERATURE_SNAPSHOT, DAILY_TEMPERATURE_FIELDS,
                                   2, displayed, false, &dailyAllocator) &&
          dailyAllocator.failures && displayed[0].buckets.size() == 28,
          "daily forecast JSON allocation failure retains data atomically");
    cache("pages.om-temp", DAILY_TEMPERATURE_SNAPSHOT);
    failNothrowAllocation = 1;
    refreshTemperatureDailyResults();
    check(displayedStatus.state == NumericHistoryState::Failed && displayedStale &&
          displayed[0].buckets.size() == 28, "daily staging failure retains data");
    ++testMillis;
    adafruitIOManager._feedCache[adafruitIOManager.findFeed("pages.om-temp")].updatedAt = testMillis;
    refreshTemperatureDailyResults();
    check(displayedStatus.state == NumericHistoryState::Ready && !displayedStale,
          "daily forecast recovers on an unchanged later feed poll");
    cache("pages.om-temp",
        "[{\"ts\":\"20261102\",\"t2m_min\":-5,\"t2m_max\":-1},"
        "{\"ts\":\"20261103\",\"t2m_min\":0,\"t2m_max\":0},"
        "{\"ts\":\"20261104\",\"t2m_min\":2.5,\"t2m_max\":2.5}]");
    refreshTemperatureDailyResults();
    check(displayedStatus.state == NumericHistoryState::Ready &&
          displayed[0].sampleCount == 3 && displayed[1].sampleCount == 3 &&
          displayed[0].buckets[1].count == 1 && displayed[1].buckets[1].count == 1 &&
          displayed[0].buckets[1].latest == 0 && displayed[1].buckets[1].latest == 0 &&
          displayed[0].minimum == -5 && displayed[1].maximum == 2.5,
          "daily recovery preserves negatives, producer-filled zeros, and equal endpoints");
    cache("pages.om-temp",
        "[{\"ts\":\"20261102\",\"t2m_min\":null,\"t2m_max\":110,\"at_min\":\"ignored\"},"
        "{\"ts\":\"20261104\",\"t2m_min\":0,\"t2m_max\":null}]");
    refreshTemperatureDailyResults();
    check(displayedStatus.state == NumericHistoryState::Ready &&
          displayed[0].buckets.size() == 2 && !displayed[0].buckets[0].count &&
          displayed[1].buckets[0].latest == 110 && !displayed[1].buckets[1].count &&
          displayed[0].buckets[1].latest == 0 &&
          displayed[0].buckets[1].startUtc - displayed[0].buckets[0].startUtc == 2 * 86400,
          "daily nulls/gaps are not zero-filled, temperatures are unbounded, apparent temps ignored");
    cache("pages.om-temp", "[]");
    refreshTemperatureDailyResults();
    check(displayedStatus.state == NumericHistoryState::Ready &&
          displayedStatus.hasData && displayed[0].buckets.empty() && displayed[1].buckets.empty(),
          "empty daily snapshot clears both series without fabricated readings");
    handleTemperatureDailyTouch(3, 0);
    check(state.currentScreen == SCREEN_TEMPERATURE_HISTORY, "daily History route");
    assertOnlyFeed("pages.temp-line");
    handleTemperatureHistoryTouch(2, 0);
    assertOnlyFeed("pages.om-temp", 60);
    handleTemperatureDailyTouch(1, 0);
    check(state.currentScreen == SCREEN_TEMPERATURES, "daily Back returns to Temperatures");

    cache("pages.p02-precipitation",
        "[\"0\",\"clear\",\"0.00\",\"0\",\"clear\",\"0.00\",\"0\",\"clear\",\"0.00\","
        "\"0\",\"clear\",\"0.00\",\"0\",\"clear\",\"0.00\",\"0\",\"clear\",\"0.00\","
        "\"15\",\"rain\",\"0.02\"]");
    enterPrecipitationScreen();
    assertOnlyFeed("pages.p02-precipitation");
    check(std::find(graphics.printed.begin(), graphics.printed.end(), "15%") !=
          graphics.printed.end(), "promoted precipitation page preserves percent formatting");
    check(navLabels[1] == "Daily", "precipitation Daily button occupies position two");
    handlePrecipitationTouch(2, 0);
    check(state.currentScreen == SCREEN_PRECIPITATION_DAILY, "precipitation Daily route");
    assertOnlyFeed("pages.om-precip", 60);
    {
        const int feed = adafruitIOManager.findFeed("pages.om-precip");
        size_t historyCount = 0;
        check(!adafruitIOManager._feedCache[feed].attempted,
              "daily precipitation fetch is immediately due on entry");
        for (size_t i = 0; i < AIO_FEED_COUNT; ++i)
        {
            auto &entry = adafruitIOManager._feedCache[i];
            entry.attempted = true;
            entry.failed = false;
            entry.lastAttempt = testMillis;
        }
        check(adafruitIOManager.findDueFeed(testMillis + 3599999U, historyCount) != feed,
              "daily precipitation does not refresh before one hour");
        for (size_t i = 0; i < AIO_FEED_COUNT; ++i)
            if (static_cast<int>(i) != feed)
                adafruitIOManager._feedCache[i].lastAttempt = testMillis + 3600000U;
        check(adafruitIOManager.findDueFeed(testMillis + 3600000U, historyCount) == feed,
              "daily precipitation refresh is due at exactly one hour");
    }
    check(displayedWindow == 28 * 86400 && displayedBucket == 86400,
          "daily precipitation uses daily forecast window");
    std::string dailyPrecipitation = "[";
    for (int i = 0; i < 28; ++i)
    {
        const time_t timestamp = 1793404800 + i * 86400;
        char date[16];
        std::strftime(date, sizeof(date), "%Y%m%d", std::gmtime(&timestamp));
        dailyPrecipitation += std::string(i ? "," : "") + "{\"ts\":\"" + date +
            "\",\"precip_hours\":" + (i == 0 ? "12" : i == 1 ? "1" : "0") +
            ",\"rain_sum\":" + (i == 0 ? "2" : "0") +
            ",\"showers_sum\":" + (i == 0 ? "1" : "0") +
            ",\"snowfall_sum\":" + (i == 0 ? "1" : "0") + "}";
    }
    dailyPrecipitation += "]";
    cache("pages.om-precip", dailyPrecipitation.c_str());
    refreshPrecipitationDailyResults();
    check(displayedStatus.hasData && !displayedStale &&
          displayed[0].buckets.size() == 28 &&
          displayed[0].buckets[0].latest == 6 &&
          displayed[1].buckets[0].latest == 9 &&
          displayed[2].buckets[0].latest == 12,
          "daily snapshot preserves 28 dates and normalizes mixed types to wet hours");
    check(!displayed[0].buckets[1].count && !displayed[1].buckets[1].count &&
          displayed[2].buckets[1].latest == 1 &&
          displayed[0].buckets[2].latest == 0 && displayed[2].buckets[2].latest == 0,
          "rounded-zero amounts retain unknown wet duration and dry dates");
    const uint32_t precipitationRevision = displayedStatus.revision;
    refreshPrecipitationDailyResults();
    check(displayedStatus.revision == precipitationRevision, "unchanged daily snapshot is not reparsed");
    for (const char *invalid : {
        "[{\"ts\":\"20261101\",\"precip_hours\":25,\"rain_sum\":0,\"showers_sum\":0,\"snowfall_sum\":0}]",
        "[{\"ts\":\"20261101\",\"precip_hours\":1,\"rain_sum\":-1,\"showers_sum\":0,\"snowfall_sum\":0}]",
        "[{\"ts\":\"20261101\",\"precip_hours\":1,\"rain_sum\":0,\"showers_sum\":0}]",
        "[{\"ts\":\"20261101\",\"precip_hours\":1,\"rain_sum\":0,\"showers_sum\":0,\"snowfall_sum\":0},"
        "{\"ts\":\"20261101\",\"precip_hours\":0,\"rain_sum\":0,\"showers_sum\":0,\"snowfall_sum\":0}]"})
    {
        cache("pages.om-precip", invalid);
        refreshPrecipitationDailyResults();
        check(displayedStale && displayedStatus.state == NumericHistoryState::Failed &&
              displayed[2].buckets.size() == 28 && displayed[2].buckets[0].latest == 12,
              "invalid daily snapshot retains the last valid data as stale");
    }
    cache("pages.om-precip", dailyPrecipitation.c_str());
    refreshPrecipitationDailyResults();
    check(!displayedStale && displayedStatus.state == NumericHistoryState::Ready,
          "daily precipitation recovers after a valid snapshot");
    testMillis += 7200001U;
    refreshPrecipitationDailyResults();
    check(displayedStale, "daily precipitation is stale after two polling intervals");
    adafruitIOManager._feedCache[adafruitIOManager.findFeed("pages.om-precip")].updatedAt = testMillis;
    refreshPrecipitationDailyResults();
    check(!displayedStale, "a fresh receipt clears daily precipitation staleness");
    cache("pages.om-precip",
        "[{\"ts\":\"20261122\",\"precip_hours\":1,\"rain_sum\":0,\"showers_sum\":0,\"snowfall_sum\":0},"
        "{\"ts\":\"20261125\",\"precip_hours\":5,\"rain_sum\":0.14,\"showers_sum\":0.01,\"snowfall_sum\":0},"
        "{\"ts\":\"20261126\",\"precip_hours\":null,\"rain_sum\":0.03,\"showers_sum\":0,\"snowfall_sum\":0}]");
    refreshPrecipitationDailyResults();
    check(displayed[2].buckets[0].latest == 1 && !displayed[0].buckets[0].count &&
          std::fabs(displayed[0].buckets[1].latest - 5.0 * 0.14 / 0.15) < 1e-5 &&
          std::fabs(displayed[1].buckets[1].latest - 5.0) < 1e-5 &&
          !displayed[2].buckets[2].count,
          "supplied rounded amounts and null hours preserve the intended durations");
    handlePrecipitationDailyTouch(1, 0);
    check(state.currentScreen == SCREEN_PRECIPITATION, "daily precipitation Back route");
    handlePrecipitationTouch(3, 0);
    assertOnlyFeed("pages.prcp-line");
    check(displayedWindow == 30 * 86400 && displayedBucket == 86400, "precipitation 30-day frame");
    cache("pages.prcp-line", "[{\"ts\":\"20261104\",\"prcp\":0.12}]");
    refreshPrecipitationHistoryResults();
    check(displayed[0].buckets.size() == 1 &&
          std::fabs(displayed[0].maximum - 0.12) < 1e-6,
          "precipitation combined snapshot reaches renderer");
    handlePrecipitationHistoryTouch(1, 0);
    check(state.currentScreen == SCREEN_PRECIPITATION, "precipitation history Back route");

    cache("pages.p03-wind", windPage);
    enterWindScreen();
    assertOnlyFeed("pages.p03-wind");
    check(graphics.printed.back() == "17", "wind displays last row from combined snapshot");
    handleWindTouch(3, 0);
    assertOnlyFeed("pages.wind-line");
    check(displayedWindow == 72 * 3600 && displayedBucket == 3600, "wind 72-hour hourly frame");
    cache("pages.wind-line", "[{\"ts\":\"2026110500\",\"speed\":4,\"gust\":null}]");
    refreshWindHistoryResults();
    check(displayedStatus.hasData && displayed[0].sampleCount == 1 &&
          displayed[1].sampleCount == 0, "null gust preserves wind speed graph");
    cache("pages.wind-line", "[{\"ts\":\"2026110500\",\"speed\":5,\"gust\":8}]");
    failNothrowAllocation = 1;
    refreshWindHistoryResults();
    check(displayedStatus.state == NumericHistoryState::Failed && displayedStale &&
          displayed[0].maximum == 4, "low-memory refresh retains stale data instead of rebooting");
    ++testMillis;
    adafruitIOManager._feedCache[adafruitIOManager.findFeed("pages.wind-line")].updatedAt = testMillis;
    refreshWindHistoryResults();
    check(displayedStatus.state == NumericHistoryState::Ready && displayed[0].maximum == 5,
          "unchanged producer data retries after a new successful poll following low memory");
    cache("pages.wind-line", "bad");
    refreshWindHistoryResults();
    check(displayedStatus.state == NumericHistoryState::Failed && displayedStale &&
          displayed[0].sampleCount == 1, "invalid wind update retains stale valid series");
    check(navLabels[2] == "Daily", "wind speed offers Daily in position three");
    handleWindHistoryTouch(3, 0);
    check(state.currentScreen == SCREEN_WIND_DAILY, "wind Daily route");
    assertOnlyFeed("pages.om-wind", 60);
    check(displayedWindow == 28U * 86400U && displayedBucket == 86400,
          "daily wind uses a daily forecast frame");
    cache("pages.om-wind", DAILY_WIND_SNAPSHOT);
    refreshWindDailyResults();
    check(displayedStatus.state == NumericHistoryState::Ready && !displayedStale &&
          displayed[0].buckets.size() == 28 && displayed[1].buckets.size() == 28 &&
          displayed[0].startUtc == firstDay && displayed[0].endUtc == lastDay + 86400,
          "daily wind includes the entire supplied date window");
    for (size_t i = 0; i < 28; ++i)
    {
        check(displayed[0].buckets[i].startUtc == firstDay + static_cast<int64_t>(i) * 86400 &&
              displayed[1].buckets[i].startUtc == displayed[0].buckets[i].startUtc,
              "daily wind dates stay aligned");
        checkNear(displayed[0].buckets[i].latest, DAILY_WIND_SPEED[i], "daily mean speed");
        checkNear(displayed[1].buckets[i].latest, DAILY_WIND_GUST[i], "daily maximum gust");
    }
    testMillis += 7200000U;
    refreshWindDailyResults();
    check(!displayedStale, "daily wind remains fresh for two polling intervals");
    ++testMillis;
    refreshWindDailyResults();
    check(displayedStale, "daily wind becomes stale after two polling intervals");
    cache("pages.om-wind",
          "[{\"ts\":\"20261111\",\"wind_speed_10m_mean\":0,\"wind_gusts_10m_max\":null},"
          "{\"ts\":\"20261113\",\"wind_speed_10m_mean\":null,\"wind_gusts_10m_max\":4}]");
    refreshWindDailyResults();
    check(displayedStatus.state == NumericHistoryState::Ready &&
          displayed[0].buckets.size() == 2 && displayed[0].buckets[0].latest == 0 &&
          !displayed[1].buckets[0].count && !displayed[0].buckets[1].count &&
          displayed[1].buckets[1].latest == 4 &&
          displayed[0].buckets[1].startUtc - displayed[0].buckets[0].startUtc == 2 * 86400,
          "daily wind preserves zeros, nulls and missing dates independently");
    const char *invalidDailyWind[] = {
        "[{\"ts\":\"20261111\",\"wind_speed_10m_mean\":2}]",
        "[{\"ts\":\"20261111\",\"wind_speed_10m_mean\":\"2\",\"wind_gusts_10m_max\":3}]",
        ("[{\"ts\":\"20261111\",\"wind_speed_10m_mean\":2,\"wind_gusts_10m_max\":3},"
         "{\"ts\":\"20261111\",\"wind_speed_10m_mean\":2,\"wind_gusts_10m_max\":3}]"),
    };
    for (const char *invalid : invalidDailyWind)
    {
        cache("pages.om-wind", invalid);
        refreshWindDailyResults();
        check(displayedStatus.state == NumericHistoryState::Failed && displayedStale &&
              displayed[0].buckets.size() == 2 && displayed[1].buckets[1].latest == 4,
              "invalid daily wind retains both prior series");
    }
    cache("pages.om-wind", "[]");
    refreshWindDailyResults();
    check(displayedStatus.state == NumericHistoryState::Ready && displayed[0].buckets.empty(),
          "empty daily wind clears prior results");
    handleWindDailyTouch(1, 0);
    check(state.currentScreen == SCREEN_WIND_HISTORY, "daily Back returns to speed history");
    assertOnlyFeed("pages.wind-line");
    handleWindHistoryTouch(2, 0);
    check(state.currentScreen == SCREEN_WIND_DIRECTION, "wind Dir route preserved");

    cache("pages.p04-baro", baroPage);
    enterBarometerScreen();
    assertOnlyFeed("pages.p04-baro");
    check(graphics.printed.back() == "steady", "barometer displays combined trend");
    handleBarometerTouch(3, 0);
    assertOnlyFeed("pages.baro-line");
    check(displayedWindow == 72 * 3600 && displayedBucket == 3600, "barometer 72-hour hourly frame");
    cache("pages.baro-line", "[{\"ts\":\"2026110513\",\"baro\":1013.09}]");
    refreshBarometerHistoryResults();
    check(displayedStatus.state == NumericHistoryState::Ready,
          "barometer snapshot reaches renderer");
    checkNear(displayed[0].maximum, 1013.09, "barometer snapshot preserves supplied pressure");
    handleBarometerHistoryTouch(1, 0);
    check(state.currentScreen == SCREEN_BAROMETER, "barometer history Back route");

    check(navLabels[1] == "Trend", "barometer position 2 is labelled Trend");
    handleBarometerTouch(2, 0);
    check(state.currentScreen == SCREEN_BAROMETER_TREND &&
          navLabels[2] == "History", "barometer Trend button and History link");
    assertOnlyFeed("pages.baro-trend");
    check(displayedWindow == 72 * 3600 && displayedBucket == 3 * 3600,
          "trend requests three-hour bucket chart without raw history");
    std::string trendPayload = "[";
    const double suppliedTrend[25][3] = {
        {1022.16, 1023.03, 1022.7911111111111},
        {1022.93, 1023.16, 1023.0188888888889},
        {1023.12, 1023.54, 1023.26125},
        {1023.18, 1023.93, 1023.6744444444444},
        {1021.67, 1023.72, 1022.7611111111112},
        {1020.58, 1021.46, 1020.9177777777778},
        {1020.36, 1021.11, 1020.7377777777778},
        {1021.16, 1021.62, 1021.3966666666666},
        {1021.26, 1021.56, 1021.39},
        {1021.22, 1021.65, 1021.3675},
        {1021.75, 1023.01, 1022.2288888888888},
        {1022.93, 1023.43, 1023.1433333333334},
        {1021.42, 1023.23, 1022.3577777777778},
        {1020.7, 1021.22, 1020.8811111111112},
        {1020.66, 1021.21, 1020.8399999999999},
        {1020.92, 1021.33, 1021.1637499999999},
        {1021.16, 1021.92, 1021.6222222222223},
        {1020.82, 1021.23, 1021.0025},
        {1020.78, 1021.12, 1020.9077777777778},
        {1021.32, 1022.17, 1021.8711111111111},
        {1020.53, 1021.78, 1021.3766666666666},
        {1018.45, 1020.17, 1019.02},
        {1018.21, 1018.8, 1018.5355555555556},
        {1018.58, 1019.33, 1018.9925000000001},
        {1018.32, 1018.62, 1018.5125},
    };
    for (size_t i = 0; i < 25; ++i)
    {
        char record[180];
        std::snprintf(record, sizeof(record),
            "%s{\"bucket\":\"2026-11-%02u-%u\",\"min\":%.17g,\"max\":%.17g,\"average\":%.17g}",
            i ? "," : "", static_cast<unsigned>(7 + (i + 7) / 8),
            static_cast<unsigned>((i + 7) % 8 + 1),
            suppliedTrend[i][0], suppliedTrend[i][1], suppliedTrend[i][2]);
        trendPayload += record;
    }
    trendPayload += "]";
    cache("pages.baro-trend", trendPayload.c_str());
    refreshBarometerTrendResults();
    check(displayedStatus.state == NumericHistoryState::Ready &&
          displayed[0].buckets.size() == 25 && displayed[2].sampleCount == 25,
          "all 25 supplied three-hour buckets and three fields are retained");
    checkNear(displayed[0].buckets[0].latest, 1022.16, "trend exact minimum");
    checkNear(displayed[1].buckets[0].latest, 1023.03, "trend exact maximum");
    checkNear(displayed[2].buckets[0].latest, 1022.7911111111111, "trend exact average");
    for (size_t i = 0; i < 25; ++i)
        for (size_t s = 0; s < 3; ++s)
            checkNear(displayed[s].buckets[i].latest, suppliedTrend[i][s],
                      "every supplied pressure min/max/average survives decoding");
    checkNear(displayed[0].minimum, 1018.21, "sample lowest low");
    checkNear(displayed[1].maximum, 1023.93, "sample highest high");
    BudgetJsonAllocator trendAllocator;
    trendAllocator.limit = 1;
    check(parseBarometerTrendHistory(trendPayload.c_str(), displayed, &trendAllocator) &&
          trendAllocator.failures && displayed[0].buckets.size() == 25,
          "JSON allocation failure preserves previous pressure trend atomically");
    const std::string gapTrend =
        "[{\"bucket\":\"2026-11-07-1\",\"min\":1,\"max\":2,\"average\":1.5},"
        "{\"bucket\":\"2026-11-07-3\",\"min\":1,\"max\":2,\"average\":1.5}]";
    static SnapshotHistoryResult gapResults[3];
    check(!parseBarometerTrendHistory(gapTrend.c_str(), gapResults) &&
          gapResults[0].buckets.size() == 2 &&
          gapResults[0].buckets[1].startUtc - gapResults[0].buckets[0].startUtc == 21600,
          "missing pressure buckets are not filled or reaggregated");
    std::string oversizedTrend = "[";
    for (size_t i = 0; i < 73; ++i)
    {
        char record[128];
        std::snprintf(record, sizeof(record),
            "%s{\"bucket\":\"2026-11-%02u-%u\",\"min\":1,\"max\":2,\"average\":1.5}",
            i ? "," : "", static_cast<unsigned>(1 + i / 8),
            static_cast<unsigned>(i % 8 + 1));
        oversizedTrend += record;
    }
    oversizedTrend += "]";
    check(parseBarometerTrendHistory(oversizedTrend.c_str(), displayed) &&
          displayed[0].buckets.size() == 25,
          "pressure bucket capacity is enforced before writing past storage");
    int64_t trendStart, trendEnd;
    check(parsePressureTrendBucket("2026-11-07-8", trendStart, trendEnd) &&
          displayed[0].startUtc == trendStart && trendEnd - trendStart == 10800 &&
          displayed[0].endUtc - displayed[0].startUtc == 25 * 10800,
          "bucket 8 is local 21:00 through midnight; no sample is trimmed");
    check(!parsePressureTrendBucket("2026-02-30-1", trendStart, trendEnd) &&
          !parsePressureTrendBucket("2026-11-07-0", trendStart, trendEnd) &&
          !parsePressureTrendBucket("2026-11-07-9", trendStart, trendEnd),
          "invalid calendar dates and bucket indices rejected");
    const char *invalidTrends[] = {
        "[{\"bucket\":\"2026-11-07-8\",\"min\":2,\"max\":1,\"average\":1.5}]",
        "[{\"bucket\":\"2026-11-07-8\",\"min\":1,\"max\":2,\"average\":3}]",
        "[{\"bucket\":\"2026-11-07-8\",\"min\":1,\"max\":2}]",
        "[{\"bucket\":\"2026-11-07-8\",\"min\":null,\"max\":2,\"average\":1.5}]",
        "[{\"bucket\":\"2026-11-07-8\",\"min\":\"1\",\"max\":2,\"average\":1.5}]",
        "[{\"bucket\":\"2026-11-07-8\",\"min\":1,\"max\":2,\"average\":1.5}] trailing",
        "[{\"bucket\":\"2026-11-07-8\",\"min\":1,\"max\":2,\"average\":1.5},"
        "{\"bucket\":\"2026-11-07-8\",\"min\":1,\"max\":2,\"average\":1.5}]",
        "[{\"bucket\":\"2026-11-07-8\",\"min\":1,\"max\":2,\"average\":1.5},"
        "{\"bucket\":\"2026-11-07-7\",\"min\":1,\"max\":2,\"average\":1.5}]",
    };
    for (const char *invalid : invalidTrends)
    {
        cache("pages.baro-trend", invalid);
        refreshBarometerTrendResults();
        check(displayedStatus.state == NumericHistoryState::Failed && displayedStale &&
              displayed[0].buckets.size() == 25,
              "invalid trend update preserves all previous buckets and marks stale");
    }
    cache("pages.baro-trend", trendPayload.c_str());
    failNothrowAllocation = 1;
    refreshBarometerTrendResults();
    check(displayedStatus.state == NumericHistoryState::Failed &&
          displayed[0].buckets.size() == 25, "trend allocation failure retains data");
    ++testMillis;
    adafruitIOManager._feedCache[adafruitIOManager.findFeed("pages.baro-trend")].updatedAt = testMillis;
    refreshBarometerTrendResults();
    check(displayedStatus.state == NumericHistoryState::Ready,
          "trend retries unchanged payload after a subsequent poll");
    cache("pages.baro-trend", "[]");
    refreshBarometerTrendResults();
    check(displayedStatus.state == NumericHistoryState::Ready &&
          displayed[0].buckets.empty(), "empty trend is successful with no invented bars");
    handleBarometerTrendTouch(3, 0);
    assertOnlyFeed("pages.baro-line");
    check(state.currentScreen == SCREEN_BAROMETER_HISTORY && navLabels[2] == "Trend",
          "Trend History link and reciprocal Trend label");
    handleBarometerHistoryTouch(3, 0);
    assertOnlyFeed("pages.baro-trend");
    check(state.currentScreen == SCREEN_BAROMETER_TREND, "History Trend link");
    handleBarometerTrendTouch(1, 0);
    assertOnlyFeed("pages.p04-baro");
    check(state.currentScreen == SCREEN_BAROMETER, "Trend Back returns to Barometer");

    cache("pages.om-sun", CLOUDS_SNAPSHOT);
    enterCloudsScreen();
    assertOnlyFeed("pages.om-sun", 60);
    {
        const int cloudFeed = adafruitIOManager.findFeed("pages.om-sun");
        size_t historyCount = 0;
        auto &entry = adafruitIOManager._feedCache[cloudFeed];
        check(!entry.attempted, "Clouds fetch is immediately due on entry");
        for (size_t i = 0; i < AIO_FEED_COUNT; ++i)
        {
            adafruitIOManager._feedCache[i].attempted = true;
            adafruitIOManager._feedCache[i].failed = false;
            adafruitIOManager._feedCache[i].lastAttempt = testMillis;
        }
        check(adafruitIOManager.findDueFeed(testMillis + 3599999U, historyCount) != cloudFeed,
              "Clouds does not refresh before one hour");
        for (size_t i = 0; i < AIO_FEED_COUNT; ++i)
            if (static_cast<int>(i) != cloudFeed)
                adafruitIOManager._feedCache[i].lastAttempt = testMillis + 3600000U;
        check(adafruitIOManager.findDueFeed(testMillis + 3600000U, historyCount) == cloudFeed,
              "Clouds refresh is due at exactly one hour");
    }
    check(state.currentScreen == SCREEN_CLOUDS &&
          displayedStatus.state == NumericHistoryState::Ready && !displayedStale &&
          displayedBucket == 86400 && displayedWindow == 28U * 86400U,
          "Clouds activates its own feed and accepts the forecast snapshot");
    int64_t cloudFirst, cloudLast;
    check(parseSnapshotDate("20261028", cloudFirst) &&
          parseSnapshotDate("20261124", cloudLast) &&
          displayed[0].startUtc == cloudFirst && displayed[0].endUtc == cloudLast + 86400,
          "Clouds includes all past and forecast dates with no leading empty window");
    for (size_t s = 0; s < 2; ++s)
        check(displayed[s].buckets.size() == 28 && displayed[s].sampleCount == 28,
              "both cloud series preserve 28 supplied dates");
    for (size_t i = 0; i < 28; ++i)
    {
        check(displayed[0].buckets[i].startUtc == cloudFirst + static_cast<int64_t>(i) * 86400,
              "cloud dates retain producer order");
        checkNear(displayed[0].buckets[i].latest, CLOUDS_SUNSHINE[i], "exact sunshine percent");
        checkNear(displayed[1].buckets[i].latest, CLOUDS_MEAN[i], "exact average cloud cover");
    }
    const uint32_t cloudRevision = displayedStatus.revision;
    refreshCloudsResults();
    check(displayedStatus.revision == cloudRevision, "unchanged cloud data is not reparsed");
    const char *invalidClouds[] = {
        "[{\"ts\":\"20260230\",\"cc_mean\":20,\"pct_ss\":50}]",
        "[{\"ts\":\"20261111\",\"cc_mean\":101,\"pct_ss\":50}]",
        "[{\"ts\":\"20261111\",\"cc_mean\":-1,\"pct_ss\":50}]",
        "[{\"ts\":\"20261111\",\"cc_mean\":20,\"pct_ss\":-1}]",
        "[{\"ts\":\"20261111\",\"cc_mean\":20,\"pct_ss\":101}]",
        "[{\"ts\":\"20261111\",\"pct_ss\":50}]",
        "[{\"ts\":\"20261111\",\"cc_mean\":20}]",
        "[{\"ts\":\"20261111\",\"cc_mean\":20,\"pct_ss\":\"50\"}]",
        "[{\"ts\":\"20261111\",\"cc_mean\":\"20\",\"pct_ss\":50}]",
        "[{\"ts\":\"20261111\",\"cc_mean\":20,\"pct_ss\":50}] trailing",
        "[{\"ts\":\"20261111\",\"cc_mean\":20,\"pct_ss\":50},"
        "{\"ts\":\"20261111\",\"cc_mean\":20,\"pct_ss\":50}]",
        "[{\"ts\":\"20261112\",\"cc_mean\":20,\"pct_ss\":50},"
        "{\"ts\":\"20261111\",\"cc_mean\":20,\"pct_ss\":50}]",
    };
    for (const char *invalid : invalidClouds)
    {
        cache("pages.om-sun", invalid);
        refreshCloudsResults();
        check(displayedStatus.state == NumericHistoryState::Failed && displayedStale &&
              displayed[0].buckets.size() == 28 &&
              std::fabs(displayed[0].buckets.back().latest - 96.4) < 0.00001 &&
              displayed[1].buckets.back().latest == 0,
              "invalid clouds update retains both previous series as stale");
    }
    BudgetJsonAllocator cloudAllocator;
    check(!parseCloudsHistory(CLOUDS_SNAPSHOT, displayed, &cloudAllocator) &&
          cloudAllocator.peak <= 8192 && cloudAllocator.live == 0,
          "cloud JSON decoding uses bounded live memory and frees it");
    std::string oversizedClouds = "[";
    for (size_t i = 0; i < 73; ++i)
    {
        const time_t date = static_cast<time_t>(cloudFirst + static_cast<int64_t>(i) * 86400);
        struct tm calendar;
        char day[16], record[128];
        check(gmtime_r(&date, &calendar) &&
              std::strftime(day, sizeof(day), "%Y%m%d", &calendar),
              "format cloud capacity fixture date");
        std::snprintf(record, sizeof(record),
            "%s{\"ts\":\"%s\",\"cc_mean\":20,\"pct_ss\":50}",
            i ? "," : "", day);
        oversizedClouds += record;
    }
    oversizedClouds += "]";
    check(parseCloudsHistory(oversizedClouds.c_str(), displayed) &&
          displayed[0].buckets.size() == 28,
          "cloud capacity overflow rejects atomically before writing beyond storage");
    cloudAllocator.limit = 1;
    check(parseCloudsHistory(CLOUDS_SNAPSHOT, displayed, &cloudAllocator) &&
          cloudAllocator.failures && displayed[0].buckets.size() == 28,
          "cloud JSON allocation failure preserves data atomically");
    cache("pages.om-sun", CLOUDS_SNAPSHOT);
    failNothrowAllocation = 1;
    refreshCloudsResults();
    check(displayedStatus.state == NumericHistoryState::Failed &&
          displayed[0].buckets.size() == 28, "cloud staging failure preserves data");
    ++testMillis;
    adafruitIOManager._feedCache[adafruitIOManager.findFeed("pages.om-sun")].updatedAt = testMillis;
    refreshCloudsResults();
    check(displayedStatus.state == NumericHistoryState::Ready && !displayedStale,
          "clouds recovers on an unchanged later feed poll");
    testMillis += 120001;
    refreshCloudsResults();
    check(!displayedStale, "Clouds stays fresh beyond the old two-minute timeout");
    testMillis += 7200000U - 120001U;
    refreshCloudsResults();
    check(!displayedStale, "Clouds remains fresh through two hourly polling intervals");
    ++testMillis;
    refreshCloudsResults();
    check(displayedStale, "Clouds becomes stale after two hours without a successful receipt");
    cache("pages.om-sun",
        "[{\"ts\":\"20261111\",\"cc_mean\":null,\"pct_ss\":0,\"cc_min\":999,\"cc_max\":\"ignored\"},"
        "{\"ts\":\"20261113\",\"cc_mean\":0,\"pct_ss\":null}]");
    refreshCloudsResults();
    check(displayedStatus.state == NumericHistoryState::Ready &&
          displayed[0].buckets.size() == 2 && !displayed[1].buckets[0].count &&
          displayed[0].buckets[0].count && displayed[0].buckets[0].latest == 0 &&
          !displayed[0].buckets[1].count && displayed[1].buckets[1].count &&
          displayed[1].buckets[1].latest == 0 &&
          displayed[0].buckets[1].startUtc - displayed[0].buckets[0].startUtc == 2 * 86400,
          "cloud series are independent, zeros remain real, min/max are ignored, dates are not filled");
    cache("pages.om-sun", "[]");
    refreshCloudsResults();
    check(displayedStatus.state == NumericHistoryState::Ready && displayed[0].buckets.empty(),
          "empty clouds snapshot clears data without inventing dates");
    handleCloudsTouch(1, 0);
    check(state.currentScreen == SCREEN_DETAILS, "Clouds Back returns to Details");

    cache("pages.p05-astro", astroPage);
    enterAstronomicalScreen();
    assertOnlyFeed("pages.p05-astro");
    check(tableValues.size() == 8 && tableValues[0] == "6:23 AM" &&
          tableValues[2] == "10:38" && tableValues[7] == "Waning Crescent",
          "Astro retains layout, local clock strings, daylight, and moon phase");
    cache("pages.p05-astro", "[]");
    refreshAstronomicalResults();
    check(tableValues[0] == "--" && tableValues[2] == "--", "bad Astro shows explicit missing values");
    handleAstronomicalTouch(1, 0);
    check(state.currentScreen == SCREEN_DETAILS, "Astro Back route");
    check(adafruitIOManager._numericSeriesCount == 0 && testUrls.empty(),
          "all snapshot graphs avoid individual numeric-history requests");
    std::printf("PASS: %zu combined weather feed checks\n", checks);
    return 0;
}
