#include "adafruitio_manager.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

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

void AdafruitIOManager::workerTask(void *parameter)
{
    static_cast<AdafruitIOManager *>(parameter)->runWorker();
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
                break;
            }
        }
        xSemaphoreGive(_mutex);

        if (selectedFeed < 0)
        {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        String value;
        int httpStatus = 0;
        uint32_t retryAfterMs = 0;
        recordRequest();
        const bool succeeded = fetchLatestValue(
            static_cast<size_t>(selectedFeed), value, httpStatus, retryAfterMs);

        xSemaphoreTake(_mutex, portMAX_DELAY);
        FeedCache &feed = _feedCache[selectedFeed];
        feed.attempted = true;
        feed.failed = !succeeded;
        feed.lastAttempt = millis();
        if (succeeded)
        {
            feed.value = value;
            feed.hasValue = true;
            feed.updatedAt = millis();
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

bool AdafruitIOManager::fetchLatestValue(
    size_t feedIndex, String &value, int &httpStatus, uint32_t &retryAfterMs)
{
    WiFiClientSecure client;
    client.setCACert(AIO_ROOT_CA);

    HTTPClient http;
    http.setConnectTimeout(REQUEST_TIMEOUT_MS);
    http.setTimeout(REQUEST_TIMEOUT_MS);

    const String url = String("https://io.adafruit.com/api/v2/") +
                       AIO_USERNAME + "/feeds/" +
                       AIO_FEEDS[feedIndex].key + "/data/last";
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
        const DeserializationError error = deserializeJson(document, http.getStream());
        if (!error && document["value"].is<const char *>())
        {
            value = document["value"].as<const char *>();
            succeeded = true;
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
