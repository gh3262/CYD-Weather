#include "app.h"
#include <HTTPClient.h>

#define private public
#include "screens/snapshot_history_page.h"
#undef private
#include "screens/text_table_page.h"
#include "screens/weather_page_values.h"
#include "screens/temperature_page_values.h"
#include "screens/precipitation_page_values.h"

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

static SnapshotHistoryResult displayed[2];
static NumericHistoryStatus displayedStatus;
static bool displayedStale = false;
static uint32_t displayedWindow = 0;
static uint32_t displayedBucket = 0;
static std::vector<std::string> tableValues;

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
void drawNav1(const char *) {}
void drawNav2(const char *) {}
void drawNav3(const char *) {}
void drawNav4(const char *) {}
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
    case SCREEN_PRECIPITATION_HISTORY: drawPrecipitationHistoryScreen(); break;
    case SCREEN_WIND: drawWindScreen(); break;
    case SCREEN_WIND_HISTORY: drawWindHistoryScreen(); break;
    case SCREEN_BAROMETER: drawBarometerScreen(); break;
    case SCREEN_BAROMETER_HISTORY: drawBarometerHistoryScreen(); break;
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

static void assertOnlyFeed(const char *key)
{
    const int feed = adafruitIOManager.findFeed(key);
    check(feed >= 0, "combined feed is registered");
    for (size_t i = 0; i < AIO_FEED_COUNT; ++i)
        check(adafruitIOManager._feedCache[i].active == (static_cast<int>(i) == feed),
              "visible page activates exactly its combined feed");
    check(AIO_FEEDS[feed].mayExceed45Chars &&
          AIO_FEEDS[feed].refreshIntervalMinutes == 1,
          "combined feeds preserve long JSON and poll once per minute");
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
static const char *dailyTemperatures =
    "[{\"ts\":\"20261004\",\"mintemp\":56.4,\"maxtemp\":83.4},"
    "{\"ts\":\"20261005\",\"mintemp\":55.2,\"maxtemp\":83.4},"
    "{\"ts\":\"20261006\",\"mintemp\":56.2,\"maxtemp\":87.2},"
    "{\"ts\":\"20261007\",\"mintemp\":55.2,\"maxtemp\":89.0},"
    "{\"ts\":\"20261008\",\"mintemp\":62.6,\"maxtemp\":89.3},"
    "{\"ts\":\"20261009\",\"mintemp\":63.5,\"maxtemp\":71.7},"
    "{\"ts\":\"20261010\",\"mintemp\":54.8,\"maxtemp\":86.9},"
    "{\"ts\":\"20261011\",\"mintemp\":61.3,\"maxtemp\":90.8},"
    "{\"ts\":\"20261012\",\"mintemp\":65.7,\"maxtemp\":90.9},"
    "{\"ts\":\"20261013\",\"mintemp\":73.6,\"maxtemp\":88.5},"
    "{\"ts\":\"20261014\",\"mintemp\":70.4,\"maxtemp\":91.0},"
    "{\"ts\":\"20261015\",\"mintemp\":53.6,\"maxtemp\":72.1},"
    "{\"ts\":\"20261016\",\"mintemp\":46.4,\"maxtemp\":64.7},"
    "{\"ts\":\"20261017\",\"mintemp\":39.0,\"maxtemp\":71.2},"
    "{\"ts\":\"20261018\",\"mintemp\":42.2,\"maxtemp\":73.2},"
    "{\"ts\":\"20261019\",\"mintemp\":45.1,\"maxtemp\":77.4},"
    "{\"ts\":\"20261020\",\"mintemp\":47.3,\"maxtemp\":79.6},"
    "{\"ts\":\"20261021\",\"mintemp\":46.6,\"maxtemp\":79.7},"
    "{\"ts\":\"20261022\",\"mintemp\":48.0,\"maxtemp\":79.3},"
    "{\"ts\":\"20261023\",\"mintemp\":46.9,\"maxtemp\":80.9},"
    "{\"ts\":\"20261024\",\"mintemp\":46.8,\"maxtemp\":77.4},"
    "{\"ts\":\"20261025\",\"mintemp\":49.9,\"maxtemp\":75.4},"
    "{\"ts\":\"20261026\",\"mintemp\":48.1,\"maxtemp\":76.3},"
    "{\"ts\":\"20261027\",\"mintemp\":48.1,\"maxtemp\":80.7},"
    "{\"ts\":\"20261028\",\"mintemp\":50.7,\"maxtemp\":81.8},"
    "{\"ts\":\"20261029\",\"mintemp\":51.1,\"maxtemp\":81.8},"
    "{\"ts\":\"20261030\",\"mintemp\":51.1,\"maxtemp\":80.2},"
    "{\"ts\":\"20261031\",\"mintemp\":59.1,\"maxtemp\":75.6},"
    "{\"ts\":\"20261101\",\"mintemp\":59.1,\"maxtemp\":63.6},"
    "{\"ts\":\"20261102\",\"mintemp\":62.6,\"maxtemp\":73.2},"
    "{\"ts\":\"20261103\",\"mintemp\":65.6,\"maxtemp\":72.7},"
    "{\"ts\":\"20261104\",\"mintemp\":51.3,\"maxtemp\":68.1}]";

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
    SnapshotHistoryResult histories[2];
    if (key == "pages.dailytemp-bar")
    {
        const SnapshotHistoryField fields[] = {
            {"mintemp", HistoryAggregation::Minimum},
            {"maxtemp", HistoryAggregation::Maximum},
        };
        return parseWeatherSnapshotHistory(payload, fields, 2, true, histories);
    }
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
    assertOnlyFeed("pages.dailytemp-bar");
    check(displayedWindow == 30 * 86400 && displayedBucket == 86400,
          "daily temperature uses a 30-day daily frame");
    cache("pages.dailytemp-bar", dailyTemperatures);
    refreshTemperatureDailyResults();
    int64_t firstDay, lastDay;
    check(parseSnapshotDate("20261006", firstDay) && parseSnapshotDate("20261104", lastDay),
          "prepare supplied daily temperature endpoints");
    check(displayedStatus.state == NumericHistoryState::Ready &&
          displayed[0].buckets.size() == 30 && displayed[1].buckets.size() == 30 &&
          displayed[0].sampleCount == 30 && displayed[1].sampleCount == 30 &&
          displayed[0].startUtc == firstDay && displayed[0].endUtc == lastDay + 86400 &&
          displayed[0].buckets.front().startUtc == firstDay &&
          displayed[1].buckets.back().startUtc == lastDay &&
          displayed[0].minimum == 39 && displayed[1].maximum == 91,
          "supplied 32-day snapshot trims to October 6 through November 4 with paired extrema");
    checkNear(displayed[0].buckets.front().latest, 56.2, "first retained low is unchanged");
    checkNear(displayed[1].buckets.back().latest, 68.1, "last supplied high is unchanged");
    const uint32_t dailyRevision = displayedStatus.revision;
    refreshTemperatureDailyResults();
    check(displayedStatus.revision == dailyRevision, "unchanged daily snapshot is not rebuilt");
    cache("pages.dailytemp-bar", "[{\"ts\":\"20261104\",\"mintemp\":5}]");
    refreshTemperatureDailyResults();
    check(displayedStatus.state == NumericHistoryState::Failed && displayedStale &&
          displayed[0].sampleCount == 30 && displayed[1].maximum == 91,
          "invalid daily snapshot retains both previous series as stale");
    cache("pages.dailytemp-bar",
        "[{\"ts\":\"20261102\",\"mintemp\":-5,\"maxtemp\":-1},"
        "{\"ts\":\"20261103\",\"mintemp\":0,\"maxtemp\":0},"
        "{\"ts\":\"20261104\",\"mintemp\":2.5,\"maxtemp\":2.5}]");
    refreshTemperatureDailyResults();
    check(displayedStatus.state == NumericHistoryState::Ready &&
          displayed[0].sampleCount == 3 && displayed[1].sampleCount == 3 &&
          displayed[0].buckets[1].count == 1 && displayed[1].buckets[1].count == 1 &&
          displayed[0].buckets[1].latest == 0 && displayed[1].buckets[1].latest == 0 &&
          displayed[0].minimum == -5 && displayed[1].maximum == 2.5,
          "daily recovery preserves negatives, producer-filled zeros, and equal endpoints");
    cache("pages.dailytemp-bar", "[]");
    refreshTemperatureDailyResults();
    check(displayedStatus.state == NumericHistoryState::Ready &&
          displayedStatus.hasData && displayed[0].buckets.empty() && displayed[1].buckets.empty(),
          "empty daily snapshot clears both series without fabricated readings");
    handleTemperatureDailyTouch(3, 0);
    check(state.currentScreen == SCREEN_TEMPERATURE_HISTORY, "daily History route");
    assertOnlyFeed("pages.temp-line");
    handleTemperatureHistoryTouch(2, 0);
    assertOnlyFeed("pages.dailytemp-bar");
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
