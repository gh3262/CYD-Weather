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
static const size_t AIO_FEED_CAPACITY = 28;

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
        bool hasValue;
        bool active;
    };

    static void workerTask(void *parameter);
    void runWorker();
    bool fetchLatestValue(size_t feedIndex, String &value, int &httpStatus);
    int findFeed(const char *feedKey) const;

    FeedCache _feedCache[AIO_FEED_CAPACITY] = {};
    SemaphoreHandle_t _mutex = nullptr;
    TaskHandle_t _worker = nullptr;
};

extern AdafruitIOManager adafruitIOManager;

#endif
