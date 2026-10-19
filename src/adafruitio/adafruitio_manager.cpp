#include "adafruitio_manager.h"
#include "numeric_history_page.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "credentials.h"

static const uint32_t FEED_POLL_INTERVAL_MS = 60000;
static const uint32_t FAILED_FEED_RETRY_MS = 30000;
static const uint32_t DEFAULT_THROTTLE_PAUSE_MS = 60000;
static const uint32_t MAX_THROTTLE_PAUSE_MS = 600000;
static const uint32_t NETWORK_RETRY_DELAY_MS = 1000;
static const uint32_t REQUEST_TIMEOUT_MS = 5000;
static const int HTTP_TOO_MANY_REQUESTS = 429;

static const char AIO_ROOT_CA[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIEjTCCA3WgAwIBAgIQDQd4KhM/xvmlcpbhMf/ReTANBgkqhkiG9w0BAQsFADBh\n"
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
    "MjAeFw0xNzExMDIxMjIzMzdaFw0yNzExMDIxMjIzMzdaMGAxCzAJBgNVBAYTAlVT\n"
    "MRUwEwYDVQQKEwxEaWdpY2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
    "b20xHzAdBgNVBAMTFkdlb1RydXN0IFRMUyBSU0EgQ0EgRzEwggEiMA0GCSqGSIb3\n"
    "DQEBAQUAA4IBDwAwggEKAoIBAQC+F+jsvikKy/65LWEx/TMkCDIuWegh1Ngwvm4Q\n"
    "yISgP7oU5d79eoySG3vOhC3w/3jEMuipoH1fBtp7m0tTpsYbAhch4XA7rfuD6whU\n"
    "gajeErLVxoiWMPkC/DnUvbgi74BJmdBiuGHQSd7LwsuXpTEGG9fYXcbTVN5SATYq\n"
    "DfbexbYxTMwVJWoVb6lrBEgM3gBBqiiAiy800xu1Nq07JdCIQkBsNpFtZbIZhsDS\n"
    "fzlGWP4wEmBQ3O67c+ZXkFr2DcrXBEtHam80Gp2SNhou2U5U7UesDL/xgLK6/0d7\n"
    "6TnEVMSUVJkZ8VeZr+IUIlvoLrtjLbqugb0T3OYXW+CQU0kBAgMBAAGjggFAMIIB\n"
    "PDAdBgNVHQ4EFgQUlE/UXYvkpOKmgP792PkA76O+AlcwHwYDVR0jBBgwFoAUTiJU\n"
    "IBiV5uNu5g/6+rkS7QYXjzkwDgYDVR0PAQH/BAQDAgGGMB0GA1UdJQQWMBQGCCsG\n"
    "AQUFBwMBBggrBgEFBQcDAjASBgNVHRMBAf8ECDAGAQH/AgEAMDQGCCsGAQUFBwEB\n"
    "BCgwJjAkBggrBgEFBQcwAYYYaHR0cDovL29jc3AuZGlnaWNlcnQuY29tMEIGA1Ud\n"
    "HwQ7MDkwN6A1oDOGMWh0dHA6Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEds\n"
    "b2JhbFJvb3RHMi5jcmwwPQYDVR0gBDYwNDAyBgRVHSAAMCowKAYIKwYBBQUHAgEW\n"
    "HGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwDQYJKoZIhvcNAQELBQADggEB\n"
    "AIIcBDqC6cWpyGUSXAjjAcYwsK4iiGF7KweG97i1RJz1kwZhRoo6orU1JtBYnjzB\n"
    "c4+/sXmnHJk3mlPyL1xuIAt9sMeC7+vreRIF5wFBC0MCN5sbHwhNN1JzKbifNeP5\n"
    "ozpZdQFmkCo+neBiKR6HqIA+LMTMCMMuv2khGGuPHmtDze4GmEGZtYLyF8EQpa5Y\n"
    "jPuV6k2Cr/N3XxFpT3hRpt/3usU/Zb9wfKPtWpoznZ4/44c1p9rzFcZYrWkj3A+7\n"
    "TNBJE0GmP2fhXhP1D/XVfIW/h0yCJGEiV9Glm/uGOa3DXHlmbAcxSyCRraG+ZBkA\n"
    "7h4SeM6Y8l/7MBRpPCz6l8Y=\n"
    "-----END CERTIFICATE-----\n";

bool AdafruitIOManager::begin()
{
    if (_worker)
        return true;

    if (AIO_USERNAME[0] == '\0' || AIO_KEY[0] == '\0' ||
        strcmp(AIO_USERNAME, "YOUR_AIO_USERNAME") == 0 ||
        strcmp(AIO_KEY, "YOUR_AIO_KEY") == 0)
    {
        Serial.println("Adafruit IO: credentials are missing or still placeholders.");
        return false;
    }

    _mutex = xSemaphoreCreateMutex();
    if (!_mutex)
    {
        Serial.println("Adafruit IO: failed to create feed cache mutex.");
        return false;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        workerTask,
        "AIOFeedPoll",
        8192,
        this,
        1,
        &_worker,
        0);
    if (created != pdPASS)
    {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
        _worker = nullptr;
        Serial.println("Adafruit IO: failed to start feed polling task.");
        return false;
    }

    Serial.println("Adafruit IO feed manager started.");
    return true;
}

bool AdafruitIOManager::isRunning() const
{
    return _worker != nullptr;
}

bool AdafruitIOManager::getWorkerStackHeadroom(size_t &bytes) const
{
    bytes = 0;
    if (!_worker)
        return false;

    bytes = uxTaskGetStackHighWaterMark(_worker);
    return true;
}

int AdafruitIOManager::findFeed(const char *feedKey) const
{
    if (!feedKey)
        return -1;

    for (size_t i = 0; i < AIO_FEED_COUNT; ++i)
    {
        if (strcmp(AIO_FEEDS[i].key, feedKey) == 0)
            return static_cast<int>(i);
    }
    return -1;
}

bool AdafruitIOManager::requestNumericHistory(
    const char *feedKey, const NumericHistoryRequest &request)
{
    const int feed = findFeed(feedKey);
    if (!_mutex || feed < 0 || !validNumericHistoryRequest(request) ||
        (AIO_FEEDS[feed].type != AdafruitIOValueType::Float &&
         AIO_FEEDS[feed].type != AdafruitIOValueType::Integer))
    {
        Serial.println("Adafruit IO: invalid numeric history request.");
        return false;
    }
    xSemaphoreTake(_mutex, portMAX_DELAY);
    const bool same = _numericFeed == feed &&
        _numericRequest.windowSeconds == request.windowSeconds &&
        _numericRequest.bucketSeconds == request.bucketSeconds &&
        _numericRequest.aggregation == request.aggregation;
    _numericFeed = feed;
    _numericRequest = request;
    ++_numericGeneration;
    _numericAttempted = false;
    if (!same)
    {
        _numericResult = {};
        _numericStatus.hasData = false;
    }
    _numericStatus.state = NumericHistoryState::Loading;
    ++_numericStatus.revision;
    ++_cacheRevision;
    xSemaphoreGive(_mutex);
    return true;
}

void AdafruitIOManager::cancelNumericHistory()
{
    if (!_mutex)
        return;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _numericFeed = -1;
    ++_numericGeneration;
    _numericStatus = {};
    _numericResult = {};
    ++_cacheRevision;
    xSemaphoreGive(_mutex);
}

bool AdafruitIOManager::getNumericHistory(
    NumericHistoryResult &result, NumericHistoryStatus &status) const
{
    if (!_mutex)
    {
        status = {};
        return false;
    }
    xSemaphoreTake(_mutex, portMAX_DELAY);
    status = _numericStatus;
    if (status.hasData)
        result = _numericResult;
    xSemaphoreGive(_mutex);
    return status.hasData;
}

bool AdafruitIOManager::setActiveFeeds(const char *const feedKeys[], size_t count)
{
    if (!_mutex || (count > 0 && !feedKeys) || count > AIO_FEED_COUNT)
        return false;

    bool selected[AIO_FEED_CAPACITY] = {};
    for (size_t i = 0; i < count; ++i)
    {
        const int feedIndex = findFeed(feedKeys[i]);
        if (feedIndex < 0)
        {
            Serial.printf("Adafruit IO: unknown feed key '%s'.\n",
                          feedKeys[i] ? feedKeys[i] : "(null)");
            return false;
        }
        selected[feedIndex] = true;
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);
    for (size_t i = 0; i < AIO_FEED_COUNT; ++i)
    {
        if (selected[i] && !_feedCache[i].active)
            _feedCache[i].attempted = false;
        _feedCache[i].active = selected[i];
    }
    xSemaphoreGive(_mutex);
    return true;
}

bool AdafruitIOManager::getFeedValue(const char *feedKey, String &value) const
{
    const int feedIndex = findFeed(feedKey);
    if (feedIndex < 0 || !_mutex)
        return false;

    xSemaphoreTake(_mutex, portMAX_DELAY);
    const bool available = _feedCache[feedIndex].hasValue;
    if (available)
        value = _feedCache[feedIndex].value;
    xSemaphoreGive(_mutex);
    return available;
}

bool AdafruitIOManager::getFeedUpdatedAt(const char *feedKey, uint32_t &updatedAt) const
{
    const int feedIndex = findFeed(feedKey);
    if (feedIndex < 0 || !_mutex)
        return false;

    xSemaphoreTake(_mutex, portMAX_DELAY);
    const bool available = _feedCache[feedIndex].hasValue;
    if (available)
        updatedAt = _feedCache[feedIndex].updatedAt;
    xSemaphoreGive(_mutex);
    return available;
}

uint32_t AdafruitIOManager::getCacheRevision() const
{
    if (!_mutex)
        return 0;

    xSemaphoreTake(_mutex, portMAX_DELAY);
    const uint32_t revision = _cacheRevision;
    xSemaphoreGive(_mutex);
    return revision;
}

void AdafruitIOManager::workerTask(void *parameter)
{
    static_cast<AdafruitIOManager *>(parameter)->runWorker();
}

bool AdafruitIOManager::setFeedHistoryCount(const char *feedKey, size_t count)
{
    const int index = findFeed(feedKey);
    if (!_mutex || index < 0 || count == 0 || count > 1000)
    {
        Serial.println("Adafruit IO: invalid history request (expected 1-1000 entries).");
        return false;
    }
    xSemaphoreTake(_mutex, portMAX_DELAY);
    FeedCache &feed = _feedCache[index];
    if (feed.historyCount != count)
    {
        feed.historyCount = count;
        feed.attempted = false;
        feed.history.clear();
        feed.cachedHistoryCount = 0;
        ++_cacheRevision;
    }
    xSemaphoreGive(_mutex);
    return true;
}

bool AdafruitIOManager::getFeedValues(
    const char *feedKey, std::vector<String> &values) const
{
    const int index = findFeed(feedKey);
    if (!_mutex || index < 0)
        return false;

    xSemaphoreTake(_mutex, portMAX_DELAY);
    const FeedCache &feed = _feedCache[index];
    const bool available = feed.cachedHistoryCount == feed.historyCount;
    if (available)
    {
        if (feed.historyCount == 1)
        {
            values.clear();
            if (feed.hasValue)
                values.push_back(feed.value);
        }
        else
            values = feed.history;
    }
    xSemaphoreGive(_mutex);
    return available;
}

bool AdafruitIOManager::getFeedHistoryEntry(
    const char *feedKey, size_t &index, String &value, size_t &count) const
{
    value = "";
    count = 0;
    const int feedIndex = findFeed(feedKey);
    if (!_mutex || feedIndex < 0)
        return false;

    xSemaphoreTake(_mutex, portMAX_DELAY);
    const FeedCache &feed = _feedCache[feedIndex];
    const bool available = feed.cachedHistoryCount == feed.historyCount;
    if (available)
    {
        count = feed.historyCount == 1
            ? (feed.hasValue ? 1 : 0) : feed.history.size();
        if (index >= count)
            index = 0;
        if (count > 0)
            value = feed.historyCount == 1 ? feed.value : feed.history[index];
    }
    xSemaphoreGive(_mutex);
    return available;
}

bool AdafruitIOManager::hasRequestSlot() const
{
    if (_requestCount < AIO_REQUESTS_PER_WINDOW)
        return true;

    // When the ring buffer is full, the next slot holds the oldest request.
    return millis() - _requestTimes[_nextRequestSlot] >= AIO_REQUEST_WINDOW_MS;
}

void AdafruitIOManager::recordRequest()
{
    _requestTimes[_nextRequestSlot] = millis();
    _nextRequestSlot = (_nextRequestSlot + 1) % AIO_REQUESTS_PER_WINDOW;
    if (_requestCount < AIO_REQUESTS_PER_WINDOW)
        ++_requestCount;
}

void AdafruitIOManager::runWorker()
{
    for (;;)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            vTaskDelay(pdMS_TO_TICKS(NETWORK_RETRY_DELAY_MS));
            continue;
        }

        if (_paused)
        {
            if (static_cast<int32_t>(millis() - _pausedUntil) < 0)
            {
                vTaskDelay(pdMS_TO_TICKS(NETWORK_RETRY_DELAY_MS));
                continue;
            }
            _paused = false;
            Serial.println("Adafruit IO: resuming requests after throttle pause.");
        }

        if (!hasRequestSlot())
        {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        const uint32_t now = millis();
        int selectedFeed = -1;
        size_t historyCount = 1;
        xSemaphoreTake(_mutex, portMAX_DELAY);
        for (size_t i = 0; i < AIO_FEED_COUNT; ++i)
        {
            const FeedCache &feed = _feedCache[i];
            const bool periodic =
                AIO_FEEDS[i].refreshPolicy == AdafruitIORefreshPolicy::Periodic;
            const bool shouldFetch = periodic || feed.active;
            const uint32_t elapsed = now - feed.lastAttempt;
            const bool due = !feed.attempted ||
                (feed.failed && elapsed >= FAILED_FEED_RETRY_MS) ||
                (periodic && elapsed >= FEED_POLL_INTERVAL_MS);
            if (shouldFetch && due)
            {
                selectedFeed = static_cast<int>(i);
                historyCount = feed.historyCount;
                break;
            }
        }
        xSemaphoreGive(_mutex);

        if (selectedFeed < 0)
        {
            processNumericHistory();
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        std::vector<String> values;
        int httpStatus = 0;
        uint32_t retryAfterMs = 0;
        recordRequest();
        const bool succeeded = fetchLatestValues(
            static_cast<size_t>(selectedFeed), historyCount, values,
            httpStatus, retryAfterMs);

        xSemaphoreTake(_mutex, portMAX_DELAY);
        FeedCache &feed = _feedCache[selectedFeed];
        // A count change during the request leaves the new request pending.
        if (feed.historyCount == historyCount)
        {
            feed.attempted = true;
            feed.failed = !succeeded;
            feed.lastAttempt = millis();
            if (succeeded)
            {
                const bool changed =
                    feed.cachedHistoryCount != historyCount ||
                    feed.hasValue != !values.empty() ||
                    (historyCount == 1
                        ? (!values.empty() && feed.value != values.front())
                        : feed.history != values);
                if (!values.empty())
                    feed.value = values.front();
                else
                    feed.value = "";
                feed.hasValue = !values.empty();
                feed.cachedHistoryCount = historyCount;
                if (historyCount > 1)
                    feed.history = std::move(values);
                feed.updatedAt = millis();
                if (changed)
                    ++_cacheRevision;
            }
        }
        xSemaphoreGive(_mutex);

        if (httpStatus == HTTP_TOO_MANY_REQUESTS)
        {
            // Cached values stay on screen while all requests wait out the limit.
            _paused = true;
            _pausedUntil = millis() + retryAfterMs;
            Serial.printf("Adafruit IO: rate limited; pausing requests for %lu s.\n",
                          static_cast<unsigned long>(retryAfterMs / 1000));
        }
        else if (!succeeded)
        {
            Serial.printf("Adafruit IO: fetch failed for '%s' (HTTP %d).\n",
                          AIO_FEEDS[selectedFeed].key, httpStatus);
        }
    }
}

void AdafruitIOManager::processNumericHistory()
{
    xSemaphoreTake(_mutex, portMAX_DELAY);
    const int feedIndex = _numericFeed;
    const uint32_t generation = _numericGeneration;
    const NumericHistoryRequest request = _numericRequest;
    const uint32_t elapsed = millis() - _numericLastAttempt;
    const bool due =
        !_numericAttempted ||
        elapsed >= (_numericStatus.state == NumericHistoryState::Failed ? FAILED_FEED_RETRY_MS
                                                                        : FEED_POLL_INTERVAL_MS);
    xSemaphoreGive(_mutex);

    if (_workGeneration != generation || feedIndex < 0)
    {
        _numericWorking = false;
        _numericAccumulator = {};
        _numericCursor = "";
        _workGeneration = generation;
    }
    if (feedIndex < 0 || (!_numericWorking && !due))
        return;

    bool succeeded = true;
    if (!_numericWorking)
    {
        const time_t now = time(nullptr);
        // Refuse a fabricated time window when the system clock is unset.
        succeeded =
            now >= 1577836800 && _numericAccumulator.begin(request, static_cast<int64_t>(now));
        if (!succeeded)
            Serial.println("Adafruit IO: numeric history requires a valid synchronized clock.");
        _numericPageCount = 0;
        _numericCursor = "";
        _numericWorking = succeeded;
    }

    int httpStatus = 0;
    uint32_t retryAfterMs = 0;
    String nextCursor;
    if (succeeded)
    {
        const NumericHistoryResult &window = _numericAccumulator.result();
        time_t start = static_cast<time_t>(window.startUtc - 1);
        time_t end = static_cast<time_t>(window.endUtc);
        struct tm utcStart, utcEnd;
        char startText[32], endText[32];
        succeeded =
            gmtime_r(&start, &utcStart) && gmtime_r(&end, &utcEnd) &&
            strftime(startText, sizeof(startText), "%Y-%m-%dT%H%%3A%M%%3A%SZ", &utcStart) > 0 &&
            strftime(endText, sizeof(endText), "%Y-%m-%dT%H%%3A%M%%3A%SZ", &utcEnd) > 0;
        if (succeeded)
        {
            String query = String("?limit=100&start_time=") + startText + "&end_time=" + endText;
            if (!_numericCursor.isEmpty())
                query += "&before=" + _numericCursor;
            recordRequest();
            succeeded =
                fetchNumericHistoryPage(static_cast<size_t>(feedIndex), query, _numericAccumulator,
                                        nextCursor, httpStatus, retryAfterMs);
            ++_numericPageCount;
            if (succeeded && !nextCursor.isEmpty() &&
                (nextCursor == _numericCursor || _numericPageCount >= 256))
            {
                Serial.println("Adafruit IO: numeric history pagination did not complete.");
                succeeded = false;
            }
        }
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);
    const bool current = _numericGeneration == generation && _numericFeed == feedIndex;
    if (current && (succeeded && !nextCursor.isEmpty()))
        _numericCursor = nextCursor;
    else
    {
        _numericWorking = false;
        if (current)
        {
            _numericAttempted = true;
            _numericLastAttempt = millis();
            if (succeeded)
            {
                _numericResult = _numericAccumulator.result();
                _numericStatus.hasData = true;
                _numericStatus.state = NumericHistoryState::Ready;
                if (_numericResult.invalidValueCount)
                    Serial.printf("Adafruit IO: skipped %lu invalid numeric readings.\n",
                                  static_cast<unsigned long>(_numericResult.invalidValueCount));
            }
            else
                _numericStatus.state = NumericHistoryState::Failed;
            ++_numericStatus.revision;
            ++_cacheRevision;
        }
    }
    xSemaphoreGive(_mutex);

    if (!_numericWorking)
        _numericAccumulator = {};
    if (!succeeded)
        Serial.printf("Adafruit IO: numeric history fetch failed for '%s' (HTTP %d).\n",
                      AIO_FEEDS[feedIndex].key, httpStatus);
    if (httpStatus == HTTP_TOO_MANY_REQUESTS)
    {
        _paused = true;
        _pausedUntil = millis() + retryAfterMs;
        Serial.println("Adafruit IO: numeric history throttled; pausing all requests.");
    }
}

bool AdafruitIOManager::fetchNumericHistoryPage(size_t feedIndex, const String &query,
                                                NumericHistoryAccumulator &accumulator,
                                                String &nextCursor, int &httpStatus,
                                                uint32_t &retryAfterMs)
{
    WiFiClientSecure client;
    client.setCACert(AIO_ROOT_CA);
    HTTPClient http;
    http.setConnectTimeout(REQUEST_TIMEOUT_MS);
    http.setTimeout(REQUEST_TIMEOUT_MS);
    http.useHTTP10(true);
    const String url = String("https://io.adafruit.com/api/v2/") + AIO_USERNAME + "/feeds/" +
                       AIO_FEEDS[feedIndex].key + "/data" + query;
    if (!http.begin(client, url))
    {
        httpStatus = -1;
        return false;
    }
    const char *headers[] = {"Retry-After", "Link"};
    http.collectHeaders(headers, 2);
    http.addHeader("X-AIO-Key", AIO_KEY);
    http.addHeader("Accept", "application/json");
    httpStatus = http.GET();
    bool succeeded = false;
    if (httpStatus == HTTP_TOO_MANY_REQUESTS)
    {
        const long seconds = http.header("Retry-After").toInt();
        retryAfterMs = seconds > 0 ? static_cast<uint32_t>(min(seconds, 600L)) * 1000
                                   : DEFAULT_THROTTLE_PAUSE_MS;
    }
    if (httpStatus == HTTP_CODE_OK)
    {
        std::string cursor;
        succeeded = readNumericHistoryPage(http.getStream(), http.header("Link").c_str(),
                                           accumulator, cursor);
        if (succeeded)
            nextCursor = cursor.c_str();
        else
            Serial.printf("Adafruit IO: invalid/incomplete numeric history page for '%s'.\n",
                          AIO_FEEDS[feedIndex].key);
    }
    http.end();
    return succeeded;
}

bool AdafruitIOManager::fetchLatestValues(
    size_t feedIndex, size_t count, std::vector<String> &values,
    int &httpStatus, uint32_t &retryAfterMs)
{
    WiFiClientSecure client;
    client.setCACert(AIO_ROOT_CA);

    HTTPClient http;
    http.setConnectTimeout(REQUEST_TIMEOUT_MS);
    http.setTimeout(REQUEST_TIMEOUT_MS);
    // Stream JSON parsing requires an unchunked HTTP response.
    http.useHTTP10(true);

    const String url = String("https://io.adafruit.com/api/v2/") +
                       AIO_USERNAME + "/feeds/" +
                       AIO_FEEDS[feedIndex].key +
                       (count == 1 ? String("/data/last")
                                   : String("/data?limit=") + String(count));
    if (!http.begin(client, url))
    {
        httpStatus = -1;
        return false;
    }

    http.addHeader("X-AIO-Key", AIO_KEY);
    http.addHeader("Accept", "application/json");
    const char *responseHeaders[] = {"Retry-After"};
    http.collectHeaders(responseHeaders, 1);
    httpStatus = http.GET();

    if (httpStatus == HTTP_TOO_MANY_REQUESTS)
    {
        const long retryAfterSeconds = http.header("Retry-After").toInt();
        retryAfterMs = retryAfterSeconds > 0
            ? min(static_cast<uint32_t>(retryAfterSeconds) * 1000, MAX_THROTTLE_PAUSE_MS)
            : DEFAULT_THROTTLE_PAUSE_MS;
    }

    bool succeeded = false;
    if (httpStatus == HTTP_CODE_OK)
    {
        JsonDocument document;
        JsonDocument filter;
        if (count == 1)
            filter["value"] = true;
        else
            filter[0]["value"] = true;
        const DeserializationError error = deserializeJson(
            document, http.getStream(), DeserializationOption::Filter(filter));
        if (!error && count == 1 && document["value"].is<const char *>())
        {
            values.push_back(String(document["value"].as<const char *>()));
            succeeded = true;
        }
        else if (!error && count > 1 && document.is<JsonArray>())
        {
            succeeded = true;
            for (JsonVariant entry : document.as<JsonArray>())
            {
                if (!entry["value"].is<const char *>() || values.size() >= count)
                {
                    Serial.printf("Adafruit IO: invalid history response for '%s'.\n",
                                  AIO_FEEDS[feedIndex].key);
                    succeeded = false;
                    break;
                }
                values.push_back(String(entry["value"].as<const char *>()));
            }
        }
        else if (error)
        {
            Serial.printf("Adafruit IO: invalid response for '%s': %s.\n",
                          AIO_FEEDS[feedIndex].key, error.c_str());
        }
        else
        {
            Serial.printf("Adafruit IO: response for '%s' has no string value.\n",
                          AIO_FEEDS[feedIndex].key);
        }
    }

    http.end();
    return succeeded;
}
