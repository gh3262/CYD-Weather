#include "app.h"

#include "adafruitio/adafruitio_manager.h"
#include "screens/text_block_page.h"

static const char *const SUMMARY_FEED_KEYS[] = {"om-current"};

static String summaryText;
static size_t summaryFirstLine = 0;
static size_t summaryLineCount = 0;
static bool summaryScrollable = false;

static void toggleSummaryPage();

static TextBlockPage summaryPage()
{
    TextBlockPage page = {
        "Summary",
        CYD_NAVY,
        summaryText.c_str(),
        TextTableColor::White,
        summaryFirstLine,
        enterSnapshotScreen,
        {nullptr, nullptr},
        {"Forecast", enterForecastScreen},
    };
    if (summaryScrollable)
    {
        page.button2 = {summaryFirstLine > 0 ? "Up" : "More", toggleSummaryPage};
    }
    return page;
}

static String readSummaryFeed()
{
    String value;
    if (!adafruitIOManager.getFeedValue("om-current", value))
        return "Loading...";
    return value;
}

static void drawSummaryPage()
{
    drawTextBlockPage(summaryPage(), &summaryLineCount);

    // Scroll buttons only appear when the text overflows the visible lines.
    const bool scrollable = summaryLineCount > TEXT_BLOCK_VISIBLE_LINES;
    if (scrollable != summaryScrollable)
    {
        summaryScrollable = scrollable;
        drawTextBlockPage(summaryPage(), &summaryLineCount);
    }
}

static void toggleSummaryPage()
{
    const size_t lastStart = summaryLineCount > TEXT_BLOCK_VISIBLE_LINES
        ? summaryLineCount - TEXT_BLOCK_VISIBLE_LINES
        : 0;
    summaryFirstLine = summaryFirstLine > 0 ? 0 : lastStart;
    refreshTextBlockPageText(summaryPage(), &summaryLineCount);
    drawNav2(summaryFirstLine > 0 ? "Up" : "More");
}

void drawSummaryScreen()
{
    if (!adafruitIOManager.setActiveFeeds(
            SUMMARY_FEED_KEYS,
            sizeof(SUMMARY_FEED_KEYS) / sizeof(SUMMARY_FEED_KEYS[0])))
    {
        Serial.println("Summary: could not activate its Adafruit IO feed.");
    }
    summaryText = readSummaryFeed();
    summaryFirstLine = 0;
    summaryScrollable = false;
    drawSummaryPage();
}

void enterSummaryScreen()
{
    state.currentScreen = SCREEN_SUMMARY;
    drawCurrentScreen();
}

void handleSummaryTouch(int x, int y)
{
    handleTextBlockPageTouch(summaryPage(), x, y);
}

void refreshSummaryResults()
{
    const String text = readSummaryFeed();
    if (text == summaryText)
        return;

    summaryText = text;
    summaryFirstLine = 0;
    drawSummaryPage();
}
