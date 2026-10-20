#include <HTTPClient.h>
#include <WiFi.h>
#include <ctime>
#include <cstdio>
#include <utility>

#define private public
#include "adafruitio_manager.h"
#undef private

TestSerial Serial;
TestWiFi WiFi;
uint32_t testMillis = 0;
std::deque<TestHttpResponse> testResponses;
std::vector<std::string> testUrls;
std::function<void()> testOnGet;
static size_t checks = 0;

static void check(bool condition, const char *message)
{
    ++checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static std::string record(const char *id, int64_t seconds, const char *value)
{
    const time_t timestamp = static_cast<time_t>(seconds);
    char text[32];
    std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&timestamp));
    return std::string("{\"id\":\"") + id + "\",\"created_at\":\"" + text +
        "\",\"value\":\"" + value + "\"}";
}

int main()
{
    AdafruitIOManager manager;
    check(manager.begin(), "start manager with fake platform");
    NumericHistoryRequest request;
    NumericHistoryResult result;
    NumericHistoryStatus status;
    check(!manager.requestNumericHistory("headline-alert", request),
          "reject text history for numeric API");
    check(!manager.requestNumericHistory("unknown", request), "reject unknown feed");
    check(manager.requestNumericHistory("feed-avg", request), "request numeric history");
    const int64_t now = std::time(nullptr);
    testResponses.push_back({200, "[" + record("a", now - 10, "80") + "]",
        {{"Link", "<https://io.adafruit.com/data>;rel=\"next\", "
                  "<https://io.adafruit.com/data?before=opaque%3D%3D>"}}});
    testResponses.push_back({200, "[" + record("a", now - 10, "80") + "," +
                                  record("b", now - 20, "60") + "]",
                             {{"Link", "<https://io.adafruit.com/data>"}}});
    manager.processNumericHistory();
    check(!manager.getNumericHistory(result, status) &&
          status.state == NumericHistoryState::Loading, "no partial result published");
    check(manager._numericWorking, "continue staging across worker turns");
    check(testUrls[0].find("limit=100&start_time=") != std::string::npos &&
          testUrls[0].find("&end_time=") != std::string::npos,
          "request uses bounded page and both time boundaries");
    check(testUrls[0].find("%3A") != std::string::npos, "UTC filters URL encoded");
    manager.processNumericHistory();
    check(manager.getNumericHistory(result, status) &&
          status.state == NumericHistoryState::Ready, "publish only completed window");
    check(result.sampleCount == 2 && result.minimum == 60 && result.maximum == 80,
          "overlapping page deduplication and raw extrema");
    check(result.endUtc - result.startUtc == 86400, "exact rolling 24h window");
    check(testUrls[1].find("&before=opaque%3D%3D") != std::string::npos,
          "opaque pagination cursor retained");
    check(manager._requestCount == 2, "each history page consumes request budget");
    check(!manager._numericWorking, "finished staging released");

    const int feed = manager.findFeed("feed-avg");
    check(manager._feedCache[feed].historyCount == 1 &&
          !manager._feedCache[feed].hasValue, "chart retrieval does not alter latest cache");
    std::vector<String> values;
    int httpStatus = 0;
    uint32_t retryAfterMs = 0;
    testResponses.push_back({200, "{\"value\":\"67.5\"}", {}});
    check(manager.fetchLatestValues(static_cast<size_t>(feed), 1, values,
                                    httpStatus, retryAfterMs) &&
          values.size() == 1 && values[0] == String("67.5"),
          "legacy latest-value response preserved");
    values.clear();
    testResponses.push_back({200, "[{\"value\":\"long alert\"},{\"value\":\"other\"}]", {}});
    check(manager.fetchLatestValues(static_cast<size_t>(feed), 2, values,
                                    httpStatus, retryAfterMs) &&
          values.size() == 2 && values[0] == String("long alert"),
          "legacy value-only alert history preserved");

    testMillis += 60001;
    testResponses.push_back({500, "", {}});
    manager.processNumericHistory();
    check(manager.getNumericHistory(result, status) &&
          status.state == NumericHistoryState::Failed && result.sampleCount == 2,
          "failed refresh retains complete chart");
    const size_t calls = testUrls.size();
    manager.processNumericHistory();
    check(testUrls.size() == calls, "failed refresh waits before retry");
    testMillis += 30000;
    testResponses.push_back({200, "[]", {{"Link", "<https://io.adafruit.com/data>"}}});
    manager.processNumericHistory();
    check(manager.getNumericHistory(result, status) && result.sampleCount == 0 &&
          status.state == NumericHistoryState::Ready,
          "successful empty window differs from failure");

    check(manager.requestNumericHistory("feed-avg", request), "start another refresh");
    testResponses.push_back({200, "[" + record("a", now - 10, "80") + "]",
        {{"Link", "<https://io.adafruit.com/data>;rel=\"next\", "
                  "<https://io.adafruit.com/data?before=page2>"}}});
    testResponses.push_back({200, "{broken json", {}});
    manager.processNumericHistory();
    manager.processNumericHistory();
    check(manager.getNumericHistory(result, status) &&
          status.state == NumericHistoryState::Failed && result.sampleCount == 0,
          "mid-pagination failure discards incomplete staging");

    check(manager.requestNumericHistory("feed-avg", request), "start cancellable fetch");
    testResponses.push_back({200, "[" + record("a", now - 10, "80") + "]", {}});
    testOnGet = [&manager]() { manager.cancelNumericHistory(); };
    manager.processNumericHistory();
    check(!manager.getNumericHistory(result, status) &&
          status.state == NumericHistoryState::Idle, "discard cancelled in-flight response");
    check(!manager._numericWorking, "cancelled staging freed");
    const size_t cancelledCalls = testUrls.size();
    manager.processNumericHistory();
    check(testUrls.size() == cancelledCalls, "cancelled job makes no further requests");

    check(manager.requestNumericHistory("feed-avg", request), "start throttled request");
    testResponses.push_back({429, "", {{"Retry-After", "45"}}});
    manager.processNumericHistory();
    check(manager._paused && manager._pausedUntil == testMillis + 45000,
          "history throttle pauses shared worker");
    check(!manager.getNumericHistory(result, status) &&
          status.state == NumericHistoryState::Failed, "throttle not an empty window");

    manager._paused = false;
    check(manager.requestNumericHistory("feed-avg", request), "pagination-loop fixture");
    const std::map<std::string, std::string> repeat = {
        {"Link", "<https://io.adafruit.com/data>;rel=\"next\", "
                 "<https://io.adafruit.com/data?before=repeated>"}};
    testResponses.push_back({200, "[" + record("a", now - 10, "80") + "]", repeat});
    testResponses.push_back({200, "[" + record("a", now - 10, "80") + "]", repeat});
    manager.processNumericHistory();
    manager.processNumericHistory();
    check(!manager.getNumericHistory(result, status) &&
          status.state == NumericHistoryState::Failed, "repeating cursor rejected");
    check(manager.requestNumericHistory("feed-avg", request), "replace in-flight request");
    testResponses.push_back({200, "[" + record("old", now - 10, "80") + "]", {}});
    testOnGet = [&manager, &request]() {
        NumericHistoryRequest replacement = request;
        replacement.bucketSeconds = 7200;
        check(manager.requestNumericHistory("feed-avg", replacement),
              "reconfigure while fetching");
    };
    manager.processNumericHistory();
    check(!manager.getNumericHistory(result, status) &&
          status.state == NumericHistoryState::Loading,
          "old generation cannot publish into replacement subscription");
    testResponses.push_back({200, "[" + record("new", now - 10, "60") + "]", {}});
    manager.processNumericHistory();
    check(manager.getNumericHistory(result, status) && result.bucketSeconds == 7200 &&
          result.minimum == 60, "replacement request publishes its own data");

    const char *const windFeeds[] = {"currentwindspeed", "currentwindgust"};
    NumericHistoryRequest windRequests[2];
    for (NumericHistoryRequest &windRequest : windRequests)
    {
        windRequest.windowSeconds = 48 * 3600;
        windRequest.bucketSeconds = 7200;
    }
    windRequests[1].aggregation = HistoryAggregation::Maximum;
    NumericHistoryRequest mismatched[2] = {windRequests[0], windRequests[1]};
    mismatched[1].bucketSeconds = 3600;
    check(!manager.requestNumericHistory(windFeeds, mismatched, 2),
          "reject series with different buckets");
    const char *const mixedFeeds[] = {"currentwindspeed", "headline-alert"};
    check(!manager.requestNumericHistory(mixedFeeds, windRequests, 2),
          "reject non-numeric series");
    check(!manager.requestNumericHistory(windFeeds, windRequests, 0) &&
          !manager.requestNumericHistory(windFeeds, windRequests,
                                         MAX_NUMERIC_HISTORY_SERIES + 1),
          "reject empty or oversized series set");

    check(manager.requestNumericHistory(windFeeds, windRequests, 2), "request two series");
    const size_t firstWindUrl = testUrls.size();
    testResponses.push_back({200, "[" + record("s1", now - 10, "12") + "]",
        {{"Link", "<https://io.adafruit.com/data>;rel=\"next\", "
                  "<https://io.adafruit.com/data?before=speed2>"}}});
    testResponses.push_back({200, "[" + record("s2", now - 4000, "4") + "]",
                             {{"Link", "<https://io.adafruit.com/data>"}}});
    testResponses.push_back({200, "[" + record("g1", now - 10, "25") + "," +
                                  record("g2", now - 20, "18") + "]",
                             {{"Link", "<https://io.adafruit.com/data>"}}});
    NumericHistoryResult windResults[2];
    manager.processNumericHistory();
    manager.processNumericHistory();
    check(!manager.getNumericHistory(windResults, 2, status) &&
          status.state == NumericHistoryState::Loading && manager._numericWorking,
          "first finished series is not published alone");
    manager.processNumericHistory();
    check(manager.getNumericHistory(windResults, 2, status) &&
          status.state == NumericHistoryState::Ready, "publish all series together");
    check(testUrls.size() == firstWindUrl + 3 &&
          testUrls[firstWindUrl].find("/feeds/currentwindspeed/") != std::string::npos &&
          testUrls[firstWindUrl + 1].find("&before=speed2") != std::string::npos &&
          testUrls[firstWindUrl + 2].find("/feeds/currentwindgust/") != std::string::npos,
          "series fetched in request order with their own pagination");
    const std::string speedQuery =
        testUrls[firstWindUrl].substr(testUrls[firstWindUrl].find('?'));
    const std::string gustQuery =
        testUrls[firstWindUrl + 2].substr(testUrls[firstWindUrl + 2].find('?'));
    check(speedQuery == gustQuery, "second series reuses the first series time window");
    check(windResults[0].sampleCount == 2 && windResults[0].maximum == 12 &&
          windResults[0].aggregation == HistoryAggregation::Average &&
          windResults[1].sampleCount == 2 && windResults[1].maximum == 25 &&
          windResults[1].aggregation == HistoryAggregation::Maximum,
          "each series keeps its own samples and aggregation");
    check(windResults[0].startUtc == windResults[1].startUtc &&
          windResults[0].endUtc == windResults[1].endUtc &&
          windResults[0].endUtc - windResults[0].startUtc == 48 * 3600 &&
          windResults[1].bucketSeconds == 7200, "series share the exact window");
    check(manager.getNumericHistory(result, status) && result.maximum == 12,
          "single-result read returns the first series");
    NumericHistoryResult tooMany[MAX_NUMERIC_HISTORY_SERIES + 1];
    check(!manager.getNumericHistory(tooMany, MAX_NUMERIC_HISTORY_SERIES + 1, status),
          "cannot read more series than requested");

    check(manager.requestNumericHistory(windFeeds, windRequests, 2), "refresh two series");
    testResponses.push_back({200, "[" + record("s3", now - 10, "30") + "]",
                             {{"Link", "<https://io.adafruit.com/data>"}}});
    testResponses.push_back({500, "", {}});
    manager.processNumericHistory();
    manager.processNumericHistory();
    check(manager.getNumericHistory(windResults, 2, status) &&
          status.state == NumericHistoryState::Failed &&
          windResults[0].maximum == 12 && windResults[1].maximum == 25,
          "later series failure keeps every previous series result");
    check(!manager._numericWorking, "failed multi-series staging released");
    check(testResponses.empty(), "all HTTP fixtures consumed");
    std::printf("PASS: %zu history manager checks\n", checks);
    return 0;
}
