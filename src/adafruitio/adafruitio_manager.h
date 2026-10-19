#ifndef ADAFRUITIO_MANAGER_H
#define ADAFRUITIO_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "numeric_history.h"

enum class NumericHistoryState { Idle, Loading, Ready, Failed };

struct NumericHistoryStatus
{
    NumericHistoryState state = NumericHistoryState::Idle;
    bool hasData = false;
    uint32_t revision = 0;
};

enum class AdafruitIOValueType
{
    Text,
    LongText,
    Integer,
    Float,
    Time
};

enum class AdafruitIORefreshPolicy
{
    Periodic,
    OnDemand
};

struct AdafruitIOFeedDefinition
{
    const char *key;
    const char *name;
    const char *category;
    AdafruitIOValueType type;
    const char *unit;
    bool mayExceed45Chars;
    AdafruitIORefreshPolicy refreshPolicy;
};

extern const AdafruitIOFeedDefinition AIO_FEEDS[];
extern const size_t AIO_FEED_COUNT;
static const size_t AIO_FEED_CAPACITY = 95;

// Adafruit IO limits data writes, not reads; this ceiling only guards against
// runaway polling. HTTP 429 responses still pause all requests.
static const size_t AIO_REQUESTS_PER_WINDOW = 60;
static const uint32_t AIO_REQUEST_WINDOW_MS = 60000;

class AdafruitIOManager
{
public:
    bool begin();
    bool isRunning() const;
    // Minimum unused worker stack since startup, in ESP32 bytes.
    bool getWorkerStackHeadroom(size_t &bytes) const;

    // Replaces the subscription set; pass an empty list to deactivate all feeds.
    bool setActiveFeeds(const char *const feedKeys[], size_t count);

    // Returns false until the active feed has produced a value.
    bool getFeedValue(const char *feedKey, String &value) const;
    bool getFeedUpdatedAt(const char *feedKey, uint32_t &updatedAt) const;
    // Changes only when cached values/history availability change, not on retries.
    uint32_t getCacheRevision() const;

    // History is newest first. The API allows up to 1000 entries per request.
    bool setFeedHistoryCount(const char *feedKey, size_t count);
    bool getFeedValues(const char *feedKey, std::vector<String> &values) const;
    // Copies only one entry. An index beyond the available entries resets to zero.
    // False means history is not ready; true with count == 0 means empty history.
    bool getFeedHistoryEntry(const char *feedKey, size_t &index,
                             String &value, size_t &count) const;

    // One chart-history subscription at a time, independent of latest/alert data.
    bool requestNumericHistory(const char *feedKey,
                               const NumericHistoryRequest &request);
    void cancelNumericHistory();
    bool getNumericHistory(NumericHistoryResult &result,
                           NumericHistoryStatus &status) const;

private:
    struct FeedCache
    {
        String value;
        uint32_t lastAttempt;
        uint32_t updatedAt;
        bool attempted;
        bool failed;
        bool hasValue;
        bool active;
        size_t historyCount = 1;
        size_t cachedHistoryCount = 0;
        std::vector<String> history;
    };

    static void workerTask(void *parameter);
    void runWorker();
    bool hasRequestSlot() const;
    void recordRequest();
    bool fetchLatestValues(size_t feedIndex, size_t count,
                          std::vector<String> &values, int &httpStatus,
                          uint32_t &retryAfterMs);
    int findFeed(const char *feedKey) const;
    void processNumericHistory();
    bool fetchNumericHistoryPage(size_t feedIndex, const String &query,
                                 NumericHistoryAccumulator &accumulator,
                                 String &nextCursor, int &httpStatus,
                                 uint32_t &retryAfterMs);

    int _numericFeed = -1;
    NumericHistoryRequest _numericRequest;
    uint32_t _numericGeneration = 0;
    NumericHistoryStatus _numericStatus;
    NumericHistoryResult _numericResult;
    uint32_t _numericLastAttempt = 0;
    bool _numericAttempted = false;
    // Only the worker accesses staging data; publish after the complete window.
    uint32_t _workGeneration = 0;
    bool _numericWorking = false;
    NumericHistoryAccumulator _numericAccumulator;
    String _numericCursor;
    size_t _numericPageCount = 0;

    FeedCache _feedCache[AIO_FEED_CAPACITY] = {};
    uint32_t _cacheRevision = 0;
    // Ring buffer of recent request times, used to stay under the rate limit.
    uint32_t _requestTimes[AIO_REQUESTS_PER_WINDOW] = {};
    size_t _requestCount = 0;
    size_t _nextRequestSlot = 0;
    uint32_t _pausedUntil = 0;
    bool _paused = false;
    SemaphoreHandle_t _mutex = nullptr;
    TaskHandle_t _worker = nullptr;
};

extern AdafruitIOManager adafruitIOManager;

#endif
