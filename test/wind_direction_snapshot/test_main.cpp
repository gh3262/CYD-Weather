#include "app.h"
#include <HTTPClient.h>
#define private public
#include "adafruitio/adafruitio_manager.h"
#undef private
#include "screens/wind_direction_snapshot.h"
#include <iostream>
#include <iterator>

TestSerial Serial;
TestWiFi WiFi;
int TestWiFi::status() const { return connected ? WL_CONNECTED : 0; }
uint32_t testMillis = 1000;
std::deque<TestHttpResponse> testResponses;
std::vector<std::string> testUrls;
std::function<void()> testOnGet;
AdafruitIOManager adafruitIOManager;
AppState state;
static TestGraphics graphics;
TestGraphics *gfx = &graphics;
static size_t checks = 0;
static int navigation = 0;

void clearScreen(uint16_t) {}
void titleBar(const char *) {}
void setDefaultFont() {}
void drawNav1(const char *) {}
void drawNav3(const char *) {}
void drawNav4(const char *) {}
bool navPressed1(int x, int) { return x == 1; }
bool navPressed3(int x, int) { return x == 3; }
void enterWindScreen() { navigation = 1; }
void enterWindHistoryScreen() { navigation = 3; }
void drawCurrentScreen() { drawWindDirectionScreen(); }

static void check(bool condition, const char *message)
{
    ++checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        for (const std::string &text : graphics.printed)
            std::fprintf(stderr, "  printed: %s\n", text.c_str());
        std::exit(1);
    }
}

static bool printed(const char *text)
{
    return std::find(graphics.printed.begin(), graphics.printed.end(), text) !=
        graphics.printed.end();
}

static std::string payload(JsonDocument &document)
{
    std::string result;
    serializeJson(document, result);
    return result;
}

static void publish(const std::string &value)
{
    std::vector<String> values = {String(value)};
    adafruitIOManager.publishLatestValues(
        adafruitIOManager.findFeed("pages.winddir-graphic"), 1, values, true);
    refreshWindDirectionResults();
}

int main(int argc, char **)
{
    if (argc == 2)
    {
        const std::string json((std::istreambuf_iterator<char>(std::cin)),
                                std::istreambuf_iterator<char>());
        WindDirectionSnapshot snapshot;
        const char *error = parseWindDirectionSnapshot(json.c_str(), snapshot);
        if (error)
        {
            std::fprintf(stderr, "FAIL: producer payload: %s\n", error);
            return 1;
        }
        std::printf("PASS: producer payload (%u windy, %u calm)\n",
            static_cast<unsigned>(snapshot.stats.windySamples),
            static_cast<unsigned>(snapshot.stats.calmSamples));
        return 0;
    }
    const char *valid =
        "{\"windowEndUtc\":\"2026-11-07T16:00:00Z\",\"windowSeconds\":43200,"
        "\"bucketSeconds\":600,\"currentBearing\":350,\"currentSpeedMph\":10,"
        "\"hasAverage\":true,\"averageBearing\":0,\"steadiness\":0.984807753,"
        "\"rangeStart\":350,\"rangeSpan\":20,\"windySamples\":2,\"calmSamples\":0,"
        "\"sectorCounts\":[2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]}";
    WindDirectionSnapshot snapshot;
    check(!parseWindDirectionSnapshot(valid, snapshot), "valid snapshot");
    check(snapshot.currentBearing == 350 && snapshot.currentSpeed == 10 &&
          snapshot.stats.maxSectorCount == 2 && snapshot.stats.rangeStart == 350 &&
          snapshot.stats.rangeSpan == 20, "exact graphic inputs and derived maximum");
    const WindDirectionSnapshot previous = snapshot;
    for (const char *field : {"windowEndUtc", "windowSeconds", "bucketSeconds",
         "currentBearing", "currentSpeedMph", "hasAverage", "averageBearing",
         "steadiness", "rangeStart", "rangeSpan", "windySamples", "calmSamples", "sectorCounts"})
    {
        JsonDocument broken;
        deserializeJson(broken, valid);
        broken.remove(field);
        check(parseWindDirectionSnapshot(payload(broken).c_str(), snapshot) &&
              snapshot.endUtc == previous.endUtc &&
              snapshot.stats.windySamples == previous.stats.windySamples,
              "missing fields rejected atomically");
    }
    for (const char *invalid : {"[]", "{}", "{", "null"})
        check(parseWindDirectionSnapshot(invalid, snapshot), "malformed payload rejected");
    for (const char *field : {"currentBearing", "currentSpeedMph", "averageBearing",
                             "steadiness", "rangeStart", "rangeSpan", "windySamples"})
    {
        JsonDocument broken;
        deserializeJson(broken, valid);
        broken[field] = -1;
        check(parseWindDirectionSnapshot(payload(broken).c_str(), snapshot),
              "negative input rejected");
        broken[field] = "NaN";
        check(parseWindDirectionSnapshot(payload(broken).c_str(), snapshot),
              "numeric strings rejected");
    }
    JsonDocument document;
    deserializeJson(document, valid);
    document["windySamples"] = 3;
    check(parseWindDirectionSnapshot(payload(document).c_str(), snapshot),
          "sector sum must equal windy count");
    document["windySamples"] = 2;
    document["calmSamples"] = 72;
    check(parseWindDirectionSnapshot(payload(document).c_str(), snapshot),
          "window cannot exceed 73 paired buckets");
    document["calmSamples"] = 0;
    document["currentBearing"] = 359.6;
    document["currentSpeedMph"] = 0.6;
    check(!parseWindDirectionSnapshot(payload(document).c_str(), snapshot) &&
          snapshot.currentBearing == 0 && snapshot.currentSpeed == 1,
          "legacy current rounding preserved");

    check(adafruitIOManager.begin(), "manager initialized");
    const char *rawFeeds[] = {"currentwinddegrees", "currentwindspeed"};
    NumericHistoryRequest requests[2];
    check(adafruitIOManager.requestNumericHistory(rawFeeds, requests, 2),
          "raw history setup before entry");
    drawWindDirectionScreen();
    const int feed = adafruitIOManager.findFeed("pages.winddir-graphic");
    for (size_t i = 0; i < AIO_FEED_COUNT; ++i)
        check(adafruitIOManager._feedCache[i].active == (i == static_cast<size_t>(feed)),
              "only graphic feed selected");
    check(adafruitIOManager._numericSeriesCount == 0 &&
          AIO_FEEDS[feed].refreshPolicy == AdafruitIORefreshPolicy::PagePeriodic &&
          AIO_FEEDS[feed].refreshIntervalMinutes == 1, "no raw history, minute snapshot polling");
    check(printed("Loading snapshot..."), "loading message before first snapshot");
    const time_t now = time(nullptr);
    char endText[32];
    struct tm utc;
    gmtime_r(&now, &utc);
    strftime(endText, sizeof(endText), "%Y-%m-%dT%H:%M:%SZ", &utc);
    document["windowEndUtc"] = endText;
    const std::string fresh = payload(document);
    publish(fresh);
    check(graphics.petals && graphics.averages && graphics.arrowColor == CYD_WHITE,
          "rose, average marker and rounded-current arrow drawn");
    const size_t redraws = graphics.redraws;
    refreshWindDirectionResults();
    publish(fresh);
    check(graphics.redraws == redraws, "unchanged payload does not redraw");
    publish("{}");
    check(printed("Stale") && graphics.arrowColor == CYD_WHITE,
          "malformed update retains prior graphic and warns");
    publish(fresh);
    check(!printed("Stale"), "valid snapshot recovers");
    testMillis += 120001;
    refreshWindDirectionResults();
    check(printed("Stale"), "expired receipt marked stale");
    publish(fresh);
    WiFi.connected = false;
    check(WiFi.status() != WL_CONNECTED, "WiFi stub disconnected");
    refreshWindDirectionResults();
    check(printed("Stale"), "WiFi loss marked stale");
    WiFi.connected = true;
    document["windowEndUtc"] = "2020-01-01T00:00:00Z";
    publish(payload(document));
    check(printed("Stale"), "old producer window marked stale despite fresh receipt");
    document["windowEndUtc"] = endText;
    document["windySamples"] = 0;
    document["calmSamples"] = 2;
    document["hasAverage"] = false;
    document["averageBearing"] = nullptr;
    document["steadiness"] = 0;
    document["rangeStart"] = 0;
    document["rangeSpan"] = 0;
    document["currentSpeedMph"] = 0;
    for (size_t i = 0; i < 16; ++i)
        document["sectorCounts"][i] = 0;
    publish(payload(document));
    check(printed("calm") && printed("Calm 100% of 12h") &&
          graphics.arrowColor == CYD_GRAY, "all-calm snapshot and gray arrow");
    document["calmSamples"] = 0;
    document["currentBearing"] = nullptr;
    document["currentSpeedMph"] = nullptr;
    publish(payload(document));
    check(printed("No data in 12h"), "successful empty window is not a fetch failure");
    handleWindDirectionTouch(1, 0);
    check(navigation == 1, "back navigation");
    handleWindDirectionTouch(3, 0);
    check(navigation == 3, "speed navigation");
    leaveWindDirectionScreen();
    const size_t stoppedRedraws = graphics.redraws;
    refreshWindDirectionResults();
    check(graphics.redraws == stoppedRedraws, "inactive screen does not refresh");
    std::printf("%u wind direction snapshot checks passed\n", static_cast<unsigned>(checks));
}
