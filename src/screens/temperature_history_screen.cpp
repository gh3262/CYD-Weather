#include "app.h"
#include "screens/history_chart_page.h"
#include "screens/temperature_hourly_history.h"

static const char *const TEMPERATURE_HOURLY_FEEDS[] = {"pages.temp-line"};
static NumericHistoryResult hourlyHistory;
static NumericHistoryStatus hourlyStatus;
static String lastHourlyPayload;
static bool hourlyPayloadParsed = false;
static bool hourlyFeedActive = false;

static bool usesHourlySnapshot()
{
    return state.currentScreen == SCREEN_TEMPERATURE_PAGE_HISTORY;
}

static HistoryChartScale temperatureHistoryScale(double, double)
{
    return {5.0, 10.0};
}

static const HistoryChartSeries TEMPERATURE_HISTORY_SERIES[] = {
    {"feed-avg", HistoryAggregation::Average, CYD_CYAN, nullptr},
};

static const HistoryChartSeries TEMPERATURE_HOURLY_SERIES[] = {
    {"pages.temp-line", HistoryAggregation::Average, CYD_CYAN, nullptr},
};

static HistoryChartPage temperatureHistoryPage()
{
    return {
        usesHourlySnapshot() ? "Last 72 Hours" : "Last 24 Hours",
        usesHourlySnapshot() ? TEMPERATURE_HOURLY_SERIES : TEMPERATURE_HISTORY_SERIES,
        1,
        usesHourlySnapshot() ? 72U * 3600U : 86400U,
        3600,
        usesHourlySnapshot() ? "72 hours" : "24 hours",
        " F",
        1,
        10.0,
        false,
        temperatureHistoryScale,
        HistoryChartStyle::Line,
    };
}

void refreshTemperatureHistoryResults()
{
    if (!usesHourlySnapshot())
    {
        refreshHistoryChartPage(temperatureHistoryPage());
        return;
    }

    String payload;
    const bool available = hourlyFeedActive &&
        adafruitIOManager.getFeedValue(TEMPERATURE_HOURLY_FEEDS[0], payload);
    if (available && (!hourlyPayloadParsed || payload != lastHourlyPayload))
    {
        const char *error = parseTemperatureHourlyHistory(payload.c_str(), hourlyHistory);
        if (error)
        {
            Serial.printf("Temps2 history: invalid pages.temp-line: %s.\n", error);
            hourlyStatus.state = NumericHistoryState::Failed;
        }
        else
        {
            hourlyStatus.hasData = true;
            hourlyStatus.state = NumericHistoryState::Ready;
        }
        ++hourlyStatus.revision;
        lastHourlyPayload = payload;
        hourlyPayloadParsed = true;
    }
    uint32_t updatedAt = 0;
    const bool stale = hourlyStatus.state == NumericHistoryState::Failed ||
        WiFi.status() != WL_CONNECTED ||
        (hourlyStatus.hasData &&
         (!available ||
          !adafruitIOManager.getFeedUpdatedAt(TEMPERATURE_HOURLY_FEEDS[0], updatedAt) ||
          millis() - updatedAt > 120000U ||
          (hourlyHistory.sampleCount > 0 &&
           static_cast<int64_t>(time(nullptr)) > hourlyHistory.endUtc + 3600)));
    refreshHistoryChartData(temperatureHistoryPage(), &hourlyHistory, hourlyStatus, stale);
}

void drawTemperatureHistoryScreen()
{
    if (usesHourlySnapshot())
    {
        hourlyHistory = {};
        hourlyStatus = {};
        hourlyStatus.state = NumericHistoryState::Loading;
        hourlyPayloadParsed = false;
        lastHourlyPayload = "";
        adafruitIOManager.cancelNumericHistory();
        drawHistoryChartFrame(temperatureHistoryPage());
        hourlyFeedActive = adafruitIOManager.setActiveFeeds(TEMPERATURE_HOURLY_FEEDS, 1);
        if (!hourlyFeedActive)
        {
            Serial.println("Temps2 history: could not activate pages.temp-line.");
            hourlyStatus.state = NumericHistoryState::Failed;
        }
        refreshTemperatureHistoryResults();
    }
    else
        drawHistoryChartPage(temperatureHistoryPage());
    drawNav2("Daily");
}

void enterTemperatureHistoryScreen()
{
    state.currentScreen = SCREEN_TEMPERATURE_HISTORY;
    drawCurrentScreen();
}

void enterTemperaturePageHistoryScreen()
{
    state.currentScreen = SCREEN_TEMPERATURE_PAGE_HISTORY;
    drawCurrentScreen();
}

void handleTemperatureHistoryTouch(int x, int y)
{
    if (navPressed1(x, y))
    {
        if (usesHourlySnapshot())
            enterTemperaturePageScreen();
        else
            enterTemperaturesScreen();
    }
    else if (navPressed2(x, y))
        enterTemperatureDailyScreen();
}
