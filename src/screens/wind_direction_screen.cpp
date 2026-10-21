#include "app.h"

#include <Fonts/FreeMono9pt7b.h>
#include <math.h>
#include <time.h>
#include <vector>

#include "adafruitio/adafruitio_manager.h"
#include "screens/wind_direction_stats.h"

static const char *const DIRECTION_FEED = "currentwinddegrees";
static const char *const SPEED_FEED = "currentwindspeed";
static const uint32_t WINDOW_SECONDS = 12U * 3600U;
static const uint32_t BUCKET_SECONDS = 10U * 60U;
static const char *const WINDOW_LABEL = "12h";
static const double CALM_MPH = 1.0;

static const int DIAL_X = 86;
static const int DIAL_Y = 116;
static const int DIAL_R = 64;
static const int PETAL_R = DIAL_R - 6;
static const int LABEL_R = DIAL_R + 12;
static const int PANEL_X = 172;

static const uint16_t ROSE_COLOR = 0x2AD4;
static const uint16_t CURRENT_COLOR = CYD_WHITE;
static const uint16_t AVERAGE_COLOR = CYD_RED;
static const uint16_t RANGE_COLOR = CYD_YELLOW;
static const uint16_t STEADY_COLOR = CYD_CYAN;

struct WindDirectionView
{
    int currentBearing = -1;
    int currentSpeed = -1;
    bool hasStats = false;
    WindDirectionStats stats;
    String status;
    uint16_t statusColor = COLOR_TEXT;
};

static bool directionActive = false;
static bool historyRequested = false;
static bool directionRendered = false;
static WindDirectionView renderedView;

static bool sameView(const WindDirectionView &a, const WindDirectionView &b)
{
    if (a.currentBearing != b.currentBearing || a.currentSpeed != b.currentSpeed ||
        a.hasStats != b.hasStats || a.status != b.status ||
        a.statusColor != b.statusColor)
        return false;
    if (!a.hasStats)
        return true;
    const WindDirectionStats &x = a.stats;
    const WindDirectionStats &y = b.stats;
    if (x.windySamples != y.windySamples || x.calmSamples != y.calmSamples ||
        x.hasAverage != y.hasAverage ||
        lround(x.averageBearing) != lround(y.averageBearing) ||
        lround(x.steadiness * 100) != lround(y.steadiness * 100) ||
        lround(x.rangeStart) != lround(y.rangeStart) ||
        lround(x.rangeSpan) != lround(y.rangeSpan))
        return false;
    for (size_t i = 0; i < WIND_ROSE_SECTORS; ++i)
        if (x.sectorCounts[i] != y.sectorCounts[i])
            return false;
    return true;
}

static bool parseNumber(const String &text, double &value)
{
    String trimmed = text;
    trimmed.trim();
    if (trimmed.isEmpty())
        return false;
    char *end = nullptr;
    value = strtod(trimmed.c_str(), &end);
    return end && *end == '\0' && isfinite(value);
}

static bool readFeedNumber(const char *feedKey, double &value)
{
    String text;
    return adafruitIOManager.getFeedValue(feedKey, text) && parseNumber(text, value);
}

static int roundBearing(double bearing)
{
    return static_cast<int>(lround(bearing)) % 360;
}

static void dialPoint(double bearing, double radius, int &x, int &y)
{
    const double radians = bearing * PI / 180.0;
    x = static_cast<int>(lround(DIAL_X + radius * sin(radians)));
    y = static_cast<int>(lround(DIAL_Y - radius * cos(radians)));
}

// Arduino_GFX arcs start at 3 o'clock and run clockwise; split wrapped arcs.
static void fillBearingArc(double fromBearing, double span, int outerR, int innerR,
                           uint16_t color)
{
    if (span >= 359.5)
    {
        gfx->fillArc(DIAL_X, DIAL_Y, outerR, innerR, 0, 359.9, color);
        return;
    }
    double start = fmod(fromBearing - 90.0 + 720.0, 360.0);
    double end = start + span;
    if (end > 360.0)
    {
        gfx->fillArc(DIAL_X, DIAL_Y, outerR, innerR, start, 359.9, color);
        start = 0.0;
        end -= 360.0;
    }
    gfx->fillArc(DIAL_X, DIAL_Y, outerR, innerR, start, end, color);
}

static void drawRose(const WindDirectionStats &stats)
{
    if (stats.maxSectorCount == 0)
        return;
    const double sectorDegrees = 360.0 / WIND_ROSE_SECTORS;
    for (size_t i = 0; i < WIND_ROSE_SECTORS; ++i)
    {
        if (stats.sectorCounts[i] == 0)
            continue;
        const int length = max(4, static_cast<int>(lround(
            PETAL_R * static_cast<double>(stats.sectorCounts[i]) / stats.maxSectorCount)));
        const double center = i * sectorDegrees;
        fillBearingArc(center - sectorDegrees / 2.0 + 1.5, sectorDegrees - 3.0,
                       length, 1, ROSE_COLOR);
    }
}

static void drawDialFace()
{
    gfx->drawCircle(DIAL_X, DIAL_Y, DIAL_R, CYD_GRAY);
    gfx->drawCircle(DIAL_X, DIAL_Y, DIAL_R / 2, CYD_DARK_GRAY);
    for (int i = 0; i < 16; ++i)
    {
        const int length = i % 4 == 0 ? 8 : i % 2 == 0 ? 5 : 3;
        int x0, y0, x1, y1;
        dialPoint(i * 22.5, DIAL_R, x0, y0);
        dialPoint(i * 22.5, DIAL_R - length, x1, y1);
        gfx->drawLine(x0, y0, x1, y1, CYD_LIGHT_GRAY);
    }
    setDefaultFont();
    gfx->setTextSize(1);
    const char *const labels[] = {"N", "E", "S", "W"};
    for (int i = 0; i < 4; ++i)
    {
        int x, y;
        dialPoint(i * 90.0, LABEL_R, x, y);
        gfx->setTextColor(i == 0 ? CYD_WHITE : CYD_LIGHT_GRAY);
        gfx->setCursor(x - 2, y - 3);
        gfx->print(labels[i]);
    }
}

static void drawRangeArc(const WindDirectionStats &stats)
{
    const double minimumSpan = 4.0;
    const double span = max(stats.rangeSpan, minimumSpan);
    const double start = stats.rangeStart - (span - stats.rangeSpan) / 2.0;
    fillBearingArc(start, span, DIAL_R + 5, DIAL_R + 2, RANGE_COLOR);
}

static void drawAverageMarker(double bearing)
{
    int tipX, tipY, leftX, leftY, rightX, rightY;
    dialPoint(bearing, DIAL_R - 14, tipX, tipY);
    dialPoint(bearing - 8.0, DIAL_R + 1, leftX, leftY);
    dialPoint(bearing + 8.0, DIAL_R + 1, rightX, rightY);
    gfx->fillTriangle(tipX, tipY, leftX, leftY, rightX, rightY, AVERAGE_COLOR);
}

// Points into the wind, like a weather vane: the tip is the direction it comes from.
static void drawCurrentArrow(double bearing, uint16_t color)
{
    const double radians = bearing * PI / 180.0;
    const double perpX = cos(radians);
    const double perpY = sin(radians);
    int tailX, tailY, neckX, neckY, tipX, tipY;
    dialPoint(bearing + 180.0, DIAL_R * 0.55, tailX, tailY);
    dialPoint(bearing, DIAL_R - 18, neckX, neckY);
    dialPoint(bearing, DIAL_R - 3, tipX, tipY);
    for (int offset = -1; offset <= 1; ++offset)
    {
        const int dx = static_cast<int>(lround(offset * perpX));
        const int dy = static_cast<int>(lround(offset * perpY));
        gfx->drawLine(tailX + dx, tailY + dy, neckX + dx, neckY + dy, color);
    }
    const int headX = static_cast<int>(lround(7 * perpX));
    const int headY = static_cast<int>(lround(7 * perpY));
    gfx->fillTriangle(tipX, tipY, neckX + headX, neckY + headY,
                      neckX - headX, neckY - headY, color);
    gfx->fillCircle(DIAL_X, DIAL_Y, 4, color);
}

static void printDegreeMark(uint16_t color)
{
    const int x = gfx->getCursorX() + 3;
    const int y = gfx->getCursorY();
    gfx->drawCircle(x, y - 10, 2, color);
    gfx->setCursor(x + 5, y);
}

static void printBearing(int bearing, uint16_t color)
{
    gfx->print(windCompassPoint(bearing));
    gfx->print(' ');
    gfx->print(bearing);
    printDegreeMark(color);
}

static void panelLine(int baseline, uint16_t color)
{
    gfx->setTextColor(color);
    gfx->setCursor(PANEL_X, baseline);
}

static void drawPanel(const WindDirectionView &view)
{
    gfx->setFont(FONT_MONO_SMALL);
    gfx->setTextSize(1);

    panelLine(47, CURRENT_COLOR);
    gfx->print("Now ");
    if (view.currentSpeed < 0)
        gfx->print("--");
    else if (view.currentSpeed < CALM_MPH)
        gfx->print("calm");
    else
        gfx->print(String(view.currentSpeed) + " mph");
    panelLine(65, CURRENT_COLOR);
    if (view.currentBearing < 0)
        gfx->print("--");
    else
        printBearing(view.currentBearing, CURRENT_COLOR);

    const WindDirectionStats &stats = view.stats;
    panelLine(89, AVERAGE_COLOR);
    gfx->print(String(WINDOW_LABEL) + " avg");
    panelLine(107, AVERAGE_COLOR);
    if (view.hasStats && stats.hasAverage)
        printBearing(roundBearing(stats.averageBearing), AVERAGE_COLOR);
    else
        gfx->print(view.hasStats && stats.windySamples == 0 && stats.calmSamples
                   ? "calm" : "--");

    panelLine(131, RANGE_COLOR);
    gfx->print("Range");
    if (view.hasStats && stats.windySamples)
    {
        gfx->print(' ');
        gfx->print(static_cast<int>(lround(stats.rangeSpan)));
        printDegreeMark(RANGE_COLOR);
    }
    panelLine(149, RANGE_COLOR);
    if (view.hasStats && stats.windySamples)
    {
        gfx->print(windCompassPoint(stats.rangeStart));
        gfx->print('-');
        gfx->print(windCompassPoint(stats.rangeStart + stats.rangeSpan));
    }
    else
        gfx->print("--");

    panelLine(173, STEADY_COLOR);
    gfx->print("Steady ");
    if (view.hasStats && stats.windySamples)
        gfx->print(String(static_cast<int>(lround(stats.steadiness * 100))) + "%");
    else
        gfx->print("--");

    setDefaultFont();
    gfx->setTextSize(1);
    gfx->setTextColor(view.statusColor);
    gfx->setCursor(PANEL_X, 186);
    gfx->print(view.status);
}

static void renderView(const WindDirectionView &view)
{
    gfx->fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, NAV_Y - HEADER_HEIGHT,
                  COLOR_BACKGROUND);
    if (view.hasStats)
        drawRose(view.stats);
    drawDialFace();
    if (view.hasStats && view.stats.windySamples)
        drawRangeArc(view.stats);
    if (view.hasStats && view.stats.hasAverage)
        drawAverageMarker(view.stats.averageBearing);
    if (view.currentBearing >= 0)
        drawCurrentArrow(view.currentBearing,
                         view.currentSpeed >= 0 && view.currentSpeed < CALM_MPH
                             ? CYD_GRAY : CURRENT_COLOR);
    drawPanel(view);
    gfx->setTextSize(2);
}

// Newest history sample, falling back to the latest-value cache before it loads.
static bool currentBearing(const NumericHistoryResult *direction, double &bearing)
{
    int64_t newest = INT64_MIN;
    for (size_t i = 0; direction && i < direction->buckets.size(); ++i)
    {
        const NumericHistoryBucket &bucket = direction->buckets[i];
        if (bucket.count && bucket.latestAtMs > newest)
        {
            newest = bucket.latestAtMs;
            bearing = bucket.latest;
        }
    }
    return newest != INT64_MIN ||
        (readFeedNumber(DIRECTION_FEED, bearing) && bearing >= 0 && bearing <= 360);
}

void refreshWindDirectionResults()
{
    if (!directionActive)
        return;
    NumericHistoryResult histories[2];
    NumericHistoryStatus status;
    const bool hasData = adafruitIOManager.getNumericHistory(histories, 2, status);
    const int64_t now = static_cast<int64_t>(time(nullptr));
    const bool stale = status.state == NumericHistoryState::Failed ||
        WiFi.status() != WL_CONNECTED ||
        (hasData && now > histories[0].endUtc + 120);

    WindDirectionView view;
    double value;
    if (currentBearing(hasData ? &histories[0] : nullptr, value))
        view.currentBearing = roundBearing(value);
    if (readFeedNumber(SPEED_FEED, value) && value >= 0)
        view.currentSpeed = static_cast<int>(lround(value));

    if (hasData && histories[0].buckets.size() == histories[1].buckets.size())
    {
        // Pair each bucket's mean bearing with that bucket's mean speed.
        std::vector<WindDirectionSample> samples;
        samples.reserve(histories[0].buckets.size());
        for (size_t i = 0; i < histories[0].buckets.size(); ++i)
        {
            const NumericHistoryBucket &direction = histories[0].buckets[i];
            const NumericHistoryBucket &speed = histories[1].buckets[i];
            if (direction.count && speed.count)
                samples.push_back({direction.value(HistoryAggregation::CircularAverage),
                                   speed.value(HistoryAggregation::Average)});
        }
        view.hasStats = true;
        view.stats = calculateWindDirectionStats(samples.data(), samples.size(), CALM_MPH);
    }

    const size_t sampleCount = view.stats.windySamples + view.stats.calmSamples;
    if (!historyRequested)
        view.status = "History unavailable";
    else if (!hasData)
        view.status = status.state == NumericHistoryState::Failed
            ? "History failed" : stale ? "Offline" : "Loading history...";
    else if (stale)
    {
        view.status = "Stale";
        view.statusColor = COLOR_WARNING;
    }
    else if (sampleCount == 0)
        view.status = String("No data in ") + WINDOW_LABEL;
    else if (view.stats.calmSamples)
        view.status = "Calm " +
            String(static_cast<int>(lround(100.0 * view.stats.calmSamples / sampleCount))) +
            "% of " + WINDOW_LABEL;

    if (directionRendered && sameView(view, renderedView))
        return;
    renderView(view);
    renderedView = view;
    directionRendered = true;
}

void drawWindDirectionScreen()
{
    const char *const feeds[] = {DIRECTION_FEED, SPEED_FEED};
    NumericHistoryRequest requests[2];
    for (NumericHistoryRequest &request : requests)
    {
        request.windowSeconds = WINDOW_SECONDS;
        request.bucketSeconds = BUCKET_SECONDS;
    }
    requests[0].aggregation = HistoryAggregation::CircularAverage;
    requests[1].aggregation = HistoryAggregation::Average;

    historyRequested = adafruitIOManager.requestNumericHistory(feeds, requests, 2);
    if (!adafruitIOManager.setActiveFeeds(feeds, 2))
        Serial.println("Wind Direction: could not activate its Adafruit IO feeds.");
    if (!historyRequested)
        Serial.println("Wind Direction: could not request Adafruit IO history.");

    clearScreen(COLOR_BACKGROUND);
    titleBar("Wind Direction");
    drawNav1("Back");
    drawNav3("Speed");
    drawNav4("Home");

    directionActive = true;
    directionRendered = false;
    refreshWindDirectionResults();
}

void leaveWindDirectionScreen()
{
    if (!directionActive)
        return;
    adafruitIOManager.cancelNumericHistory();
    directionActive = false;
}

void enterWindDirectionScreen()
{
    state.currentScreen = SCREEN_WIND_DIRECTION;
    drawCurrentScreen();
}

void handleWindDirectionTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterWindScreen();
    else if (navPressed3(x, y))
        enterWindHistoryScreen();
}
