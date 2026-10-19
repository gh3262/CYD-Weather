#include "app.h"

#include <math.h>
#include <utility>
#include <vector>

#include "adafruitio/adafruitio_manager.h"
#include "widgets/line_chart_widget.h"

static const size_t TEMPERATURE_HISTORY_COUNT = 200;
static const char *const HISTORY_FEEDS[] = {"feed-avg"};
static bool historyActive = false;
static bool historyReady = false;
static bool forceHistoryRender = true;
static std::vector<String> renderedHistory;
static LineChartPoint historyPoints[TEMPERATURE_HISTORY_COUNT];

static void drawHistoryMessage(const char *message)
{
    gfx->fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, NAV_Y - HEADER_HEIGHT,
                  COLOR_BACKGROUND);
    setDefaultFont();
    centerText(message, 105, COLOR_TEXT, 1);
    gfx->setTextSize(2);
}

void refreshTemperatureHistoryResults()
{
    if (!historyReady)
        return;

    std::vector<String> values;
    if (!adafruitIOManager.getFeedValues("feed-avg", values))
        return;
    if (!forceHistoryRender && values == renderedHistory)
        return;

    if (values.size() > TEMPERATURE_HISTORY_COUNT)
    {
        Serial.println("Temperature history: response exceeds requested count.");
        drawHistoryMessage("Unexpected history size");
        return;
    }

    double minimum = INFINITY;
    double maximum = -INFINITY;
    size_t invalidCount = 0;
    for (size_t i = 0; i < values.size(); ++i)
    {
        float temperature;
        const bool valid = parseTemperatureFeedValue(
            values[values.size() - 1 - i], temperature);
        historyPoints[i] = {
            static_cast<double>(i),
            valid ? static_cast<double>(temperature) : NAN,
        };
        if (valid)
        {
            minimum = fmin(minimum, temperature);
            maximum = fmax(maximum, temperature);
        }
        else
            ++invalidCount;
    }
    if (invalidCount > 0)
        Serial.printf("Temperature history: %u invalid readings shown as gaps.\n",
                      static_cast<unsigned int>(invalidCount));

    if (values.empty() || !isfinite(minimum))
    {
        drawHistoryMessage(values.empty()
            ? "No temperature history" : "No valid temperature readings");
    }
    else
    {
        LineChartWidgetOptions chart;
        chart.x = 0;
        chart.y = HEADER_HEIGHT + 16;
        chart.width = SCREEN_WIDTH;
        chart.height = NAV_Y - chart.y - 14;
        chart.minimumX = 0;
        chart.maximumX = values.size() > 1 ? values.size() - 1 : 1;
        chart.minimumY = minimum - 10.0;
        chart.maximumY = maximum + 10.0;
        chart.showYScale = true;
        chart.yTickInterval = 5.0;
        chart.yLabelInterval = 10.0;
        if (!drawLineChartWidget(chart, historyPoints, values.size()))
        {
            drawHistoryMessage("Could not draw history");
            return;
        }

        gfx->fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, 16, COLOR_BACKGROUND);
        gfx->fillRect(0, NAV_Y - 14, SCREEN_WIDTH, 14, COLOR_BACKGROUND);
        setDefaultFont();
        gfx->setTextSize(1);
        gfx->setTextColor(COLOR_TEXT);
        gfx->setCursor(4, HEADER_HEIGHT + 3);
        gfx->print("Min " + String(minimum, 1) + " F  Max " +
                   String(maximum, 1) + " F");
        gfx->setCursor(4, NAV_Y - 11);
        gfx->print("Older");
        const String count = String(static_cast<unsigned int>(values.size())) +
            " samples";
        centerText(count.c_str(), NAV_Y - 11, COLOR_TEXT, 1);
        gfx->setCursor(SCREEN_WIDTH - 34, NAV_Y - 11);
        gfx->print("Newer");
        gfx->setTextSize(2);
    }

    renderedHistory = std::move(values);
    forceHistoryRender = false;
}

void drawTemperatureHistoryScreen()
{
    historyActive = true;
    historyReady = adafruitIOManager.setFeedHistoryCount(
        "feed-avg", TEMPERATURE_HISTORY_COUNT);
    const bool activated = adafruitIOManager.setActiveFeeds(HISTORY_FEEDS, 1);
    historyReady = historyReady && activated;
    if (!historyReady)
        Serial.println("Temperature history: could not request Adafruit IO history.");

    clearScreen(COLOR_BACKGROUND);
    titleBar("Last 24 Hours");
    drawNav1("Back");
    drawNav4("Home");
    drawHistoryMessage(historyReady
        ? "Loading history..." : "History unavailable");
    forceHistoryRender = true;
    refreshTemperatureHistoryResults();
}

void leaveTemperatureHistoryScreen()
{
    if (!historyActive)
        return;

    if (!adafruitIOManager.setFeedHistoryCount("feed-avg", 1))
        Serial.println("Temperature history: could not restore latest-value polling.");
    historyActive = false;
    historyReady = false;
    std::vector<String>().swap(renderedHistory);
}

void enterTemperatureHistoryScreen()
{
    state.currentScreen = SCREEN_TEMPERATURE_HISTORY;
    drawCurrentScreen();
}

void handleTemperatureHistoryTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterTemperaturesScreen();
}
