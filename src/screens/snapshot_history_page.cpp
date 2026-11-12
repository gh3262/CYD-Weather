#include "app.h"
#include "screens/snapshot_history_page.h"
#include "screens/barometer_trend_history.h"
#include "screens/clouds_history.h"

static SnapshotHistoryResult snapshotHistories[PRESSURE_TREND_SERIES_COUNT];
static NumericHistoryStatus snapshotStatus;
static const char *snapshotFeed = nullptr;
static const SnapshotHistoryField *snapshotFields = nullptr;
static uint32_t lastSnapshotRevision = 0;
static uint32_t lastSnapshotAttemptAt = 0;
static bool snapshotParsed = false;
static bool snapshotFeedActive = false;
static uint32_t snapshotStaleAfterMs = 120000U;

void refreshSnapshotHistoryPage(const HistoryChartPage &page)
{
    uint32_t feedRevision = 0;
    uint32_t updatedAt = 0;
    const bool available = snapshotFeedActive &&
        adafruitIOManager.getFeedValueRevision(snapshotFeed, feedRevision) &&
        adafruitIOManager.getFeedUpdatedAt(snapshotFeed, updatedAt);
    const bool daily = page.style == HistoryChartStyle::DailyBars ||
        page.style == HistoryChartStyle::DailyFloatingBars;
    const bool trend = page.style == HistoryChartStyle::PressureTrendBars;
    const bool clouds = page.style == HistoryChartStyle::CloudRangeLine;
    const bool dailyForecast = page.style == HistoryChartStyle::DailyForecastFloatingBars;
    if (available && (!snapshotParsed || feedRevision != lastSnapshotRevision ||
        (snapshotStatus.state == NumericHistoryState::Failed &&
         updatedAt != lastSnapshotAttemptAt)))
    {
        String payload;
        const char *error = nullptr;
        if (!adafruitIOManager.getFeedValue(snapshotFeed, payload))
            error = "could not copy snapshot from feed cache";
        else if (trend)
            error = parseBarometerTrendHistory(payload.c_str(), snapshotHistories);
        else if (clouds)
            error = parseCloudsHistory(payload.c_str(), snapshotHistories);
        else if (dailyForecast)
            error = parseDailyForecastHistory(
                payload.c_str(), snapshotFields, page.seriesCount, snapshotHistories);
        else
            error = parseWeatherSnapshotHistory(
                payload.c_str(), snapshotFields, page.seriesCount, daily, snapshotHistories);
        if (error)
        {
            Serial.printf("%s: invalid %s: %s.\n", page.title, snapshotFeed, error);
            snapshotStatus.state = NumericHistoryState::Failed;
        }
        else
        {
            snapshotStatus.hasData = true;
            snapshotStatus.state = NumericHistoryState::Ready;
        }
        ++snapshotStatus.revision;
        lastSnapshotRevision = feedRevision;
        lastSnapshotAttemptAt = updatedAt;
        snapshotParsed = true;
    }
    int64_t dailyEnd = 0;
    const int64_t now = static_cast<int64_t>(time(nullptr));
    const bool oldWindow = daily
        ? !completedHistoryEndUtc(now, dailyEnd) || dailyEnd > snapshotHistories[0].endUtc
        : !clouds && !dailyForecast && snapshotHistories[0].endUtc &&
            now > snapshotHistories[0].endUtc + page.bucketSeconds;
    const bool stale = snapshotStatus.state == NumericHistoryState::Failed ||
        WiFi.status() != WL_CONNECTED ||
        (snapshotStatus.hasData &&
         (!available ||
          millis() - updatedAt > snapshotStaleAfterMs || oldWindow));
    refreshHistoryChartData(page, snapshotHistories, snapshotStatus, stale);
}

void drawSnapshotHistoryPage(const HistoryChartPage &page, const char *feedKey,
                             const SnapshotHistoryField fields[])
{
    for (SnapshotHistoryResult &history : snapshotHistories)
    {
        history.buckets.resize(0);
        history.sampleCount = 0;
        history.endUtc = 0;
    }
    snapshotStatus = {};
    snapshotStatus.state = NumericHistoryState::Loading;
    snapshotFeed = feedKey;
    for (size_t i = 0; i < AIO_FEED_COUNT; ++i)
        if (strcmp(AIO_FEEDS[i].key, feedKey) == 0)
        {
            snapshotStaleAfterMs =
                static_cast<uint32_t>(AIO_FEEDS[i].refreshIntervalMinutes) * 2U * 60000U;
            break;
        }
    snapshotFields = fields;
    snapshotParsed = false;
    adafruitIOManager.cancelNumericHistory();
    drawHistoryChartFrame(page);
    const char *feeds[] = {feedKey};
    snapshotFeedActive = adafruitIOManager.setActiveFeeds(feeds, 1);
    if (!snapshotFeedActive)
    {
        Serial.printf("%s: could not activate %s.\n", page.title, feedKey);
        snapshotStatus.state = NumericHistoryState::Failed;
    }
    refreshSnapshotHistoryPage(page);
}
