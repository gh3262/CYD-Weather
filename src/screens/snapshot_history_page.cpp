#include "app.h"
#include "screens/snapshot_history_page.h"

static NumericHistoryResult snapshotHistories[MAX_NUMERIC_HISTORY_SERIES];
static NumericHistoryStatus snapshotStatus;
static const char *snapshotFeed = nullptr;
static const SnapshotHistoryField *snapshotFields = nullptr;
static String lastSnapshotPayload;
static bool snapshotParsed = false;
static bool snapshotFeedActive = false;

void refreshSnapshotHistoryPage(const HistoryChartPage &page)
{
    String payload;
    const bool available = snapshotFeedActive &&
        adafruitIOManager.getFeedValue(snapshotFeed, payload);
    const bool daily = page.style == HistoryChartStyle::DailyBars;
    if (available && (!snapshotParsed || payload != lastSnapshotPayload))
    {
        const char *error = parseWeatherSnapshotHistory(
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
        lastSnapshotPayload = payload;
        snapshotParsed = true;
    }
    uint32_t updatedAt = 0;
    int64_t dailyEnd = 0;
    const int64_t now = static_cast<int64_t>(time(nullptr));
    const bool oldWindow = daily
        ? !completedHistoryEndUtc(now, dailyEnd) || dailyEnd > snapshotHistories[0].endUtc
        : snapshotHistories[0].endUtc && now > snapshotHistories[0].endUtc + 3600;
    const bool stale = snapshotStatus.state == NumericHistoryState::Failed ||
        WiFi.status() != WL_CONNECTED ||
        (snapshotStatus.hasData &&
         (!available || !adafruitIOManager.getFeedUpdatedAt(snapshotFeed, updatedAt) ||
          millis() - updatedAt > 120000U || oldWindow));
    refreshHistoryChartData(page, snapshotHistories, snapshotStatus, stale);
}

void drawSnapshotHistoryPage(const HistoryChartPage &page, const char *feedKey,
                             const SnapshotHistoryField fields[])
{
    for (NumericHistoryResult &history : snapshotHistories)
        history = {};
    snapshotStatus = {};
    snapshotStatus.state = NumericHistoryState::Loading;
    snapshotFeed = feedKey;
    snapshotFields = fields;
    snapshotParsed = false;
    lastSnapshotPayload = "";
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
