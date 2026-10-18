#ifndef ADAFRUITIO_MANAGER_H
#define ADAFRUITIO_MANAGER_H

#include <Arduino.h>

enum class AdafruitIOValueType
{
    Text,
    LongText,
    Integer,
    Float
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
static const size_t AIO_FEED_CAPACITY = 39;

// Adafruit IO limits data writes, not reads; this ceiling only guards against
// runaway polling. HTTP 429 responses still pause all requests.
static const size_t AIO_REQUESTS_PER_WINDOW = 60;
static const uint32_t AIO_REQUEST_WINDOW_MS = 60000;

class AdafruitIOManager
{
public:
    bool begin();
    bool isRunning() const;

    // Replaces the subscription set; pass an empty list to deactivate all feeds.
    bool setActiveFeeds(const char *const feedKeys[], size_t count);

    // Returns false until the active feed has produced a value.
    bool getFeedValue(const char *feedKey, String &value) const;
    bool getFeedUpdatedAt(const char *feedKey, uint32_t &updatedAt) const;

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
    };

    static void workerTask(void *parameter);
    void runWorker();
    bool hasRequestSlot() const;
    void recordRequest();
    bool fetchLatestValue(size_t feedIndex, String &value, int &httpStatus,
                          uint32_t &retryAfterMs);
    int findFeed(const char *feedKey) const;

    FeedCache _feedCache[AIO_FEED_CAPACITY] = {};
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
