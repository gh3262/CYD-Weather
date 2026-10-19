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
    check(testResponses.empty(), "all HTTP fixtures consumed");
    std::printf("PASS: %zu history manager checks\n", checks);
    return 0;
}
