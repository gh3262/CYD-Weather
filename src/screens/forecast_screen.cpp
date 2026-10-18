#include "app.h"

#include "adafruitio/adafruitio_manager.h"
#include "screens/text_block_page.h"

static const char *const FORECAST_FEED_KEYS[] = {
    "nws.00", "nws.01", "nws.02", "nws.03", "nws.04",
    "nws.05", "nws.06", "nws.07", "nws.08", "nws.09",
    "nws.10", "nws.11", "nws.12", "nws.13",
};

static const size_t FORECAST_COUNT =
    sizeof(FORECAST_FEED_KEYS) / sizeof(FORECAST_FEED_KEYS[0]);
static const size_t FORECAST_INITIAL_LOAD = 5;
// How many forecasts past the current one to keep loaded in the travel direction.
static const size_t FORECAST_PREFETCH = 2;

static size_t forecastIndex = 0;
static String forecastText;
static size_t forecastFirstLine = 0;
static size_t forecastLineCount = 0;
static bool forecastScrollable = false;
static bool forecastLoaded[FORECAST_COUNT] = {};
static char forecastTitle[24];

static void showPreviousForecast();
static void showNextForecast();
static void showTodayForecast();
static void showMoreForecast();

// Adds feeds to the active set; feeds already loaded are not fetched again.
static void loadForecasts(size_t start, size_t count, bool forward)
{
    bool added = false;
    for (size_t i = 0; i < count; ++i)
    {
        const size_t index = forward
            ? (start + i) % FORECAST_COUNT
            : (start + FORECAST_COUNT - i) % FORECAST_COUNT;
        if (!forecastLoaded[index])
        {
            forecastLoaded[index] = true;
            added = true;
        }
    }
    if (!added)
        return;

    const char *keys[FORECAST_COUNT];
    size_t keyCount = 0;
    for (size_t i = 0; i < FORECAST_COUNT; ++i)
    {
        if (forecastLoaded[i])
            keys[keyCount++] = FORECAST_FEED_KEYS[i];
    }
    if (!adafruitIOManager.setActiveFeeds(keys, keyCount))
        Serial.println("Forecast: could not activate its Adafruit IO feeds.");
}

// NWS nighttime periods start with "Tonight", "Overnight" or "<Day> Night",
// so only the first two words are checked to ignore "overnight" later on.
static bool isNightForecast()
{
    String text = forecastText;
    text.trim();
    int end = text.indexOf(' ');
    if (end >= 0)
    {
        const int second = text.indexOf(' ', end + 1);
        end = second >= 0 ? second : text.length();
    }
    else
    {
        end = text.length();
    }
    String opening = text.substring(0, end);
    opening.toLowerCase();
    return opening.indexOf("night") >= 0;
}

static TextBlockPage forecastPage()
{
    snprintf(forecastTitle, sizeof(forecastTitle), "Forecast %u/%u",
             static_cast<unsigned>(forecastIndex + 1),
             static_cast<unsigned>(FORECAST_COUNT));
    TextBlockPage page = {
        forecastTitle,
        static_cast<uint16_t>(isNightForecast() ? CYD_DARK_GRAY : CYD_DARK_GREEN),
        forecastText.c_str(),
        TextTableColor::White,
        forecastFirstLine,
        showPreviousForecast,
        {"Next", showNextForecast},
        {"Today", showTodayForecast},
        forecastScrollable ? showMoreForecast : nullptr,
    };
    return page;
}

static String readForecastFeed()
{
    String value;
    if (!adafruitIOManager.getFeedValue(FORECAST_FEED_KEYS[forecastIndex], value))
        return "Loading...";
    return value;
}

static void drawForecastPage()
{
    forecastText = readForecastFeed();
    forecastFirstLine = 0;
    drawTextBlockPage(forecastPage(), &forecastLineCount);

    // The More button only appears when the text overflows the visible lines.
    const bool scrollable = forecastLineCount > TEXT_BLOCK_VISIBLE_LINES;
    if (scrollable != forecastScrollable)
    {
        forecastScrollable = scrollable;
        drawTextBlockPage(forecastPage(), &forecastLineCount);
    }
}

static void showPreviousForecast()
{
    forecastIndex = (forecastIndex + FORECAST_COUNT - 1) % FORECAST_COUNT;
    loadForecasts(forecastIndex, FORECAST_PREFETCH + 1, false);
    drawForecastPage();
}

static void showNextForecast()
{
    forecastIndex = (forecastIndex + 1) % FORECAST_COUNT;
    loadForecasts(forecastIndex, FORECAST_PREFETCH + 1, true);
    drawForecastPage();
}

static void showTodayForecast()
{
    forecastIndex = 0;
    loadForecasts(0, FORECAST_PREFETCH + 1, true);
    drawForecastPage();
}

// Pages down, keeping one line of context, and wraps back to the top.
static void showMoreForecast()
{
    const size_t step = TEXT_BLOCK_VISIBLE_LINES - 1;
    forecastFirstLine = forecastFirstLine + TEXT_BLOCK_VISIBLE_LINES < forecastLineCount
        ? forecastFirstLine + step
        : 0;
    refreshTextBlockPageText(forecastPage(), &forecastLineCount);
}

void drawForecastScreen()
{
    for (size_t i = 0; i < FORECAST_COUNT; ++i)
        forecastLoaded[i] = false;
    loadForecasts(0, FORECAST_INITIAL_LOAD, true);

    forecastIndex = 0;
    forecastScrollable = false;
    drawForecastPage();
}

void enterForecastScreen()
{
    state.currentScreen = SCREEN_FORECAST;
    drawCurrentScreen();
}

void handleForecastTouch(int x, int y)
{
    handleTextBlockPageTouch(forecastPage(), x, y);
}

void refreshForecastResults()
{
    if (readForecastFeed() != forecastText)
        drawForecastPage();
}
