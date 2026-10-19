#include "app.h"

#include "adafruitio/adafruitio_manager.h"
#include "screens/text_block_page.h"

static String alertText;
static String alertTitle;
static long alertCount = 0;
static size_t detailIndex = 0;
static size_t detailCount = 0;
static size_t firstLine = 0;
static size_t lineCount = 0;
static bool requestFailed = false;

static void previousAlert();
static void nextAlert();
static void pageAlertText();
static void scrollAlertUp();
static void scrollAlertDown();

static TextBlockPage alertPage()
{
    TextBlockPage page = {
        alertTitle.c_str(),
        COLOR_BACKGROUND,
        alertText.c_str(),
        TextTableColor::White,
        firstLine,
        enterHomeScreen,
        {"Details", enterAlertDetailsScreen},
        {"Full Alert", enterFullAlertScreen},
        pageAlertText,
    };
    if (state.currentScreen == SCREEN_ALERT_DETAILS)
    {
        page.backAction = previousAlert;
        page.button2 = {"Next", nextAlert};
        page.button3 = {"Alerts", enterAlertsScreen};
    }
    else if (state.currentScreen == SCREEN_FULL_ALERT)
    {
        page.backAction = enterAlertsScreen;
        page.button2 = {"Up", scrollAlertUp};
        page.button3 = {"Down", scrollAlertDown};
        page.textTapAction = nullptr;
    }
    return page;
}

static long readAlertCount()
{
    String value;
    if (!adafruitIOManager.getFeedValue("nbr-alerts", value))
        return 0;
    const long count = value.toInt();
    return count > 0 ? count : 0;
}

static void activateAlertFeed()
{
    requestFailed = false;
    const char *key = state.currentScreen == SCREEN_ALERT_DETAILS ? "weather-alerts"
        : state.currentScreen == SCREEN_FULL_ALERT ? "big-alert" : "headline-alert";

    if (state.currentScreen == SCREEN_ALERT_DETAILS && alertCount > 0)
    {
        if (!adafruitIOManager.setFeedHistoryCount(
                key, static_cast<size_t>(alertCount)))
            requestFailed = true;
    }

    // Re-entering or a changed count requests fresh alert text.
    if (!adafruitIOManager.setActiveFeeds(nullptr, 0) ||
        (!requestFailed && alertCount > 0 &&
         !adafruitIOManager.setActiveFeeds(&key, 1)))
        requestFailed = true;

    if (requestFailed)
        Serial.println("Alerts: could not activate alert feed.");
}

static void renderAlerts(bool fullRedraw)
{
    String text;
    String title = state.currentScreen == SCREEN_ALERT_DETAILS ? "Details"
        : state.currentScreen == SCREEN_FULL_ALERT ? "Full Alert" : "Alerts";
    detailCount = 0;

    if (alertCount == 0)
        text = "No Current Alerts";
    else if (requestFailed)
        text = "Unable to request alert data. See the serial log.";
    else if (state.currentScreen == SCREEN_ALERT_DETAILS)
    {
        if (!adafruitIOManager.getFeedHistoryEntry(
                "weather-alerts", detailIndex, text, detailCount))
            text = "Loading...";
        else if (detailCount == 0)
            text = "No alert detail entries available.";
        else
        {
            title += " " + String(detailIndex + 1) + "/" + String(detailCount);
            if (detailCount < static_cast<size_t>(alertCount))
                text += "\n\nOnly " + String(detailCount) + " of " +
                        String(alertCount) + " alert entries available.";
        }
    }
    else
    {
        const char *key = state.currentScreen == SCREEN_FULL_ALERT
            ? "big-alert" : "headline-alert";
        if (!adafruitIOManager.getFeedValue(key, text))
            text = "Loading...";
        else if (state.currentScreen == SCREEN_ALERTS)
            text += " - " + String(alertCount) +
                    (alertCount == 1 ? " Alert" : " Alerts");
    }

    if (!fullRedraw && text == alertText && title == alertTitle)
        return;

    firstLine = 0;
    const bool titleChanged = title != alertTitle;
    alertText = text;
    alertTitle = title;
    if (fullRedraw || titleChanged)
        drawTextBlockPage(alertPage(), &lineCount);
    else
        refreshTextBlockPageText(alertPage(), &lineCount);
}

static void showAlertEntry(size_t index)
{
    detailIndex = index;
    renderAlerts(true);
}

static void previousAlert()
{
    if (detailCount > 0)
        showAlertEntry((detailIndex + detailCount - 1) % detailCount);
}

static void nextAlert()
{
    if (detailCount > 0)
        showAlertEntry((detailIndex + 1) % detailCount);
}

static void scrollAlertTo(size_t line)
{
    if (line == firstLine)
        return;
    firstLine = line;
    refreshTextBlockPageText(alertPage(), &lineCount);
}

static void pageAlertText()
{
    scrollAlertTo(firstLine + TEXT_BLOCK_VISIBLE_LINES < lineCount
        ? firstLine + TEXT_BLOCK_VISIBLE_LINES - 1 : 0);
}

static void scrollAlertUp()
{
    const size_t step = TEXT_BLOCK_VISIBLE_LINES - 1;
    scrollAlertTo(firstLine > step ? firstLine - step : 0);
}

static void scrollAlertDown()
{
    const size_t last = lineCount > TEXT_BLOCK_VISIBLE_LINES
        ? lineCount - TEXT_BLOCK_VISIBLE_LINES : 0;
    scrollAlertTo(min(firstLine + TEXT_BLOCK_VISIBLE_LINES - 1, last));
}

void drawAlertsScreen()
{
    alertCount = readAlertCount();
    detailIndex = 0;
    activateAlertFeed();
    renderAlerts(true);
}

void enterAlertsScreen()
{
    state.currentScreen = SCREEN_ALERTS;
    drawCurrentScreen();
}

void handleAlertsTouch(int x, int y)
{
    handleTextBlockPageTouch(alertPage(), x, y);
}

void refreshAlertsResults()
{
    const long count = readAlertCount();
    if (count != alertCount)
    {
        alertCount = count;
        detailIndex = 0;
        activateAlertFeed();
    }
    renderAlerts(false);
}

void enterAlertDetailsScreen()
{
    state.currentScreen = SCREEN_ALERT_DETAILS;
    drawCurrentScreen();
}

void enterFullAlertScreen()
{
    state.currentScreen = SCREEN_FULL_ALERT;
    drawCurrentScreen();
}
