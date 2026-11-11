#include "app.h"
#include "screens/snapshot_history_page.h"
#include "screens/barometer_trend_history.h"

static SnapshotHistoryResult snapshotHistories[PRESSURE_TREND_SERIES_COUNT];
static NumericHistoryStatus snapshotStatus;
static const char *snapshotFeed = nullptr;
static const SnapshotHistoryField *snapshotFields = nullptr;
static uint32_t lastSnapshotRevision = 0;
static uint32_t lastSnapshotAttemptAt = 0;
static bool snapshotParsed = false;
static bool snapshotFeedActive = false;

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
        : snapshotHistories[0].endUtc &&
            now > snapshotHistories[0].endUtc + page.bucketSeconds;
    const bool stale = snapshotStatus.state == NumericHistoryState::Failed ||
        WiFi.status() != WL_CONNECTED ||
        (snapshotStatus.hasData &&
         (!available ||
          millis() - updatedAt > 120000U || oldWindow));
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
