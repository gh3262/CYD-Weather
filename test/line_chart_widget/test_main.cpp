#include "app.h"
#include "line_chart_widget.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

RecordingSerial Serial;
static RecordingGraphics graphics;
RecordingGraphics *gfx = &graphics;
static size_t checks = 0;

static void check(bool condition, const char *message)
{
    ++checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static LineChartWidgetOptions testOptions()
{
    LineChartWidgetOptions options;
    options.x = 10;
    options.y = 40;
    options.width = 105;
    options.height = 105;
    options.minimumX = 0;
    options.maximumX = 10;
    options.minimumY = 0;
    options.maximumY = 10;
    options.showAxes = false;
    options.showGrid = false;
    return options;
}

static void expectLine(int x0, int y0, int x1, int y1)
{
    check(graphics.lines.size() == 1, "exactly one line segment");
    const RecordedLine &line = graphics.lines[0];
    check(line.x0 == x0 && line.y0 == y0 &&
          line.x1 == x1 && line.y1 == y1, "expected clipped/mapped endpoints");
}

static void expectInvalid(const LineChartWidgetOptions &options,
                          const LineChartPoint *points, size_t count)
{
    graphics.reset();
    const size_t previousErrors = Serial.errors;
    check(!drawLineChartWidget(options, points, count), "invalid input rejected");
    check(Serial.errors == previousErrors + 1, "invalid input logged");
    check(graphics.rectangles.empty() && graphics.lines.empty() &&
          graphics.pixels.empty() && graphics.texts.empty(),
          "invalid input leaves previous chart intact");
}

int main()
{
    LineChartWidgetOptions options = testOptions();
    const LineChartPoint diagonal[] = {{0, 0}, {10, 10}};
    check(drawLineChartWidget(options, diagonal, 2), "draw diagonal");
    expectLine(12, 142, 112, 42);
    check(graphics.rectangles.size() == 1, "clear exactly one widget rectangle");
    const RecordedRect &rect = graphics.rectangles[0];
    check(rect.x == 10 && rect.y == 40 && rect.width == 105 &&
          rect.height == 105 && rect.color == options.backgroundColor,
          "clear only configured widget rectangle");

    const LineChartPoint leftClip[] = {{-5, 0}, {5, 10}};
    graphics.reset();
    check(drawLineChartWidget(options, leftClip, 2), "clip left");
    expectLine(12, 92, 62, 42);

    const LineChartPoint rightClip[] = {{5, 0}, {15, 10}};
    graphics.reset();
    check(drawLineChartWidget(options, rightClip, 2), "clip right");
    expectLine(62, 142, 112, 92);

    const LineChartPoint topClip[] = {{0, 5}, {10, 15}};
    graphics.reset();
    check(drawLineChartWidget(options, topClip, 2), "clip top");
    expectLine(12, 92, 62, 42);

    const LineChartPoint bottomClip[] = {{0, -5}, {10, 5}};
    graphics.reset();
    check(drawLineChartWidget(options, bottomClip, 2), "clip bottom");
    expectLine(62, 142, 112, 92);

    const LineChartPoint crossing[] = {{-5, 5}, {15, 5}};
    graphics.reset();
    check(drawLineChartWidget(options, crossing, 2), "both endpoints outside");
    expectLine(12, 92, 112, 92);

    const LineChartPoint outside[] = {{0, 11}, {10, 11}};
    graphics.reset();
    check(drawLineChartWidget(options, outside, 2), "offscreen segment accepted");
    check(graphics.lines.empty() && graphics.pixels.empty(), "offscreen segment hidden");

    const LineChartPoint vertical[] = {{5, -5}, {5, 15}};
    graphics.reset();
    check(drawLineChartWidget(options, vertical, 2), "duplicate X allowed");
    expectLine(62, 142, 62, 42);

    const LineChartPoint uneven[] = {{0, 0}, {1, 5}, {10, 10}};
    graphics.reset();
    check(drawLineChartWidget(options, uneven, 3), "uneven X spacing");
    check(graphics.lines.size() == 2 && graphics.lines[0].x1 == 22 &&
          graphics.lines[1].x0 == 22, "X uses values, not array indices");

    const LineChartPoint gaps[] = {{0, 1}, {2, NAN}, {4, 3}, {6, 4}, {8, NAN}};
    graphics.reset();
    check(drawLineChartWidget(options, gaps, 5), "missing readings accepted");
    expectLine(52, 112, 72, 102);
    check(graphics.pixels.size() == 3, "isolated valid reading remains visible");

    graphics.reset();
    check(drawLineChartWidget(options, nullptr, 0), "empty chart accepted");
    check(graphics.lines.empty() && graphics.pixels.empty(), "no invented zero line");
    const LineChartPoint missing[] = {{0, NAN}, {10, NAN}};
    graphics.reset();
    check(drawLineChartWidget(options, missing, 2), "all-missing chart accepted");
    check(graphics.lines.empty() && graphics.pixels.empty(), "all missing stays empty");

    const LineChartPoint single[] = {{5, 5}};
    graphics.reset();
    check(drawLineChartWidget(options, single, 1), "single point accepted");
    check(graphics.lines.empty() && graphics.pixels.size() == 1 &&
          graphics.pixels[0].x == 62 && graphics.pixels[0].y == 92,
          "single point visible");

    options.showAxes = true;
    options.showGrid = true;
    graphics.reset();
    check(drawLineChartWidget(options, nullptr, 0), "draw axes and grid");
    check(graphics.lines.size() == 8, "two axes and six quarter-grid lines");
    check(graphics.lines[0].x0 == 37 && graphics.lines[0].color == options.gridColor,
          "grid location and color");
    check(graphics.lines[6].color == options.axisColor, "axis color");

    options = testOptions();
    options.showPointMarkers = true;
    graphics.reset();
    check(drawLineChartWidget(options, diagonal, 2), "edge markers");
    for (const RecordedLine &line : graphics.lines)
    {
        check(line.x0 >= 12 && line.x0 <= 112 && line.x1 >= 12 && line.x1 <= 112 &&
              line.y0 >= 42 && line.y0 <= 142 && line.y1 >= 42 && line.y1 <= 142,
              "markers and lines stay inside plot");
    }

    options = testOptions();
    options.minimumX = 1700000000;
    options.maximumX = 1700000002;
    const LineChartPoint timestamps[] = {{1700000000, 0}, {1700000001, 10}};
    graphics.reset();
    check(drawLineChartWidget(options, timestamps, 2), "timestamp resolution");
    expectLine(12, 142, 62, 42);

    options = testOptions();
    LineChartPoint samples[100];
    for (size_t i = 0; i < 100; ++i)
        samples[i] = {static_cast<double>(i), 5.0 + std::sin(i / 10.0)};
    options.maximumX = 99;
    graphics.reset();
    check(drawLineChartWidget(options, samples, 100), "100 sample chart");
    check(graphics.lines.size() == 99, "100 samples produce 99 segments");
    options.maximumX = 10;

    expectInvalid(options, nullptr, 1);
    options.width = 7;
    expectInvalid(options, diagonal, 2);
    options = testOptions();
    options.x = -1;
    expectInvalid(options, diagonal, 2);
    options = testOptions();
    options.x = 300;
    expectInvalid(options, diagonal, 2);
    options = testOptions();
    options.y = 200;
    expectInvalid(options, diagonal, 2);
    options = testOptions();
    options.minimumY = options.maximumY;
    expectInvalid(options, diagonal, 2);
    options = testOptions();
    options.minimumX = 11;
    expectInvalid(options, diagonal, 2);
    options = testOptions();
    options.maximumX = INFINITY;
    expectInvalid(options, diagonal, 2);
    options = testOptions();
    options.minimumY = NAN;
    expectInvalid(options, diagonal, 2);
    options = testOptions();
    const LineChartPoint unordered[] = {{5, 1}, {4, 2}};
    expectInvalid(options, unordered, 2);
    const LineChartPoint invalidX[] = {{NAN, 1}};
    expectInvalid(options, invalidX, 1);
    const LineChartPoint invalidY[] = {{1, INFINITY}};
    expectInvalid(options, invalidY, 1);
    const double largest = std::numeric_limits<double>::max();
    const LineChartPoint overflow[] = {{-largest, 1}, {largest, 1}};
    options.maximumX = 1;
    expectInvalid(options, overflow, 2);
    options = testOptions();
    options.showYScale = true;
    options.minimumY = -12;
    options.maximumY = 23;
    options.labelColor = 99;
    graphics.reset();
    check(drawLineChartWidget(options, nullptr, 0), "draw negative and positive Y scale");
    check(graphics.texts.size() == 4, "labels at multiples of ten inside bounds");
    const char *expectedLabels[] = {"-10", "0", "10", "20"};
    for (size_t i = 0; i < graphics.texts.size(); ++i)
    {
        const RecordedText &text = graphics.texts[i];
        check(text.value == expectedLabels[i], "correct label value");
        check(text.color == 99, "configurable label color");
        check(text.x >= options.x && text.y >= options.y &&
              text.y + 8 <= options.y + options.height,
              "scale text stays within widget");
    }
    check(graphics.lines.size() == 8, "Y axis plus seven five-unit ticks");
    check(graphics.lines[0].x0 == 37, "gutter fits widest label");
    check(graphics.textSize == 2 && graphics.textColor == COLOR_TEXT,
          "restore standard text settings");

    options.minimumY = 0;
    options.maximumY = 10;
    options.yTickInterval = 1.25;
    options.yLabelInterval = 2.5;
    options.yLabelDecimalPlaces = 1;
    graphics.reset();
    check(drawLineChartWidget(options, diagonal, 2), "fractional custom intervals");
    check(graphics.texts.size() == 5 && graphics.texts[1].value == "2.5",
          "fractional labels");
    check(graphics.lines.size() == 11, "axis, nine custom ticks, and data segment");
    check(graphics.lines.back().x0 == 43 && graphics.lines.back().y0 == 140 &&
          graphics.lines.back().x1 == 112 && graphics.lines.back().y1 == 44,
          "data mapping uses scale gutter and vertical label margins");

    options.minimumY = -0.1;
    options.maximumY = 0.1;
    options.yTickInterval = 0.05;
    options.yLabelInterval = 0.1;
    options.yLabelDecimalPlaces = 2;
    graphics.reset();
    check(drawLineChartWidget(options, nullptr, 0), "small signed decimal scale");
    check(graphics.texts.size() == 3 && graphics.texts[1].value == "0.00",
          "zero label never negative zero");
    options.yTickInterval = 0;
    expectInvalid(options, nullptr, 0);
    options.yTickInterval = NAN;
    expectInvalid(options, nullptr, 0);
    options = testOptions();
    options.showYScale = true;
    options.yLabelInterval = INFINITY;
    expectInvalid(options, nullptr, 0);
    options.yLabelInterval = 0.01;
    expectInvalid(options, nullptr, 0);
    options.yLabelInterval = 10;
    options.yTickInterval = 0.001;
    expectInvalid(options, nullptr, 0);
    options.yTickInterval = 5;
    options.yLabelDecimalPlaces = 4;
    expectInvalid(options, nullptr, 0);
    options.yLabelDecimalPlaces = 0;
    options.width = 12;
    expectInvalid(options, nullptr, 0);

    options = testOptions();
    gfx = nullptr;
    expectInvalid(options, diagonal, 2);
    gfx = &graphics;

    options = testOptions();
    const LineChartPoint flat[] = {{0, 5}, {10, 5}};
    const LineChartSeries twoSeries[] = {{diagonal, 2, 21}, {flat, 2, 22}};
    graphics.reset();
    check(drawLineChartSeriesWidget(options, twoSeries, 2), "draw two series");
    check(graphics.rectangles.size() == 1, "series share one cleared chart");
    check(graphics.lines.size() == 2 && graphics.lines[0].color == 21 &&
          graphics.lines[1].color == 22, "each series uses its own color, in order");
    check(graphics.lines[0].x0 == 12 && graphics.lines[0].y0 == 142 &&
          graphics.lines[0].x1 == 112 && graphics.lines[0].y1 == 42,
          "first series mapped on shared axes");
    check(graphics.lines[1].y0 == 92 && graphics.lines[1].y1 == 92,
          "second series mapped on shared axes");
    check(graphics.pixels.size() == 4 && graphics.pixels[2].color == 22,
          "series point pixels use series color");

    const LineChartSeries badSecond[] = {{diagonal, 2, 21}, {unordered, 2, 22}};
    graphics.reset();
    size_t previousErrors = Serial.errors;
    check(!drawLineChartSeriesWidget(options, badSecond, 2), "any invalid series rejected");
    check(Serial.errors == previousErrors + 1 && graphics.rectangles.empty() &&
          graphics.lines.empty() && graphics.pixels.empty(),
          "invalid later series leaves previous chart intact");
    const LineChartSeries missingBuffer[] = {{diagonal, 2, 21}, {nullptr, 2, 22}};
    graphics.reset();
    previousErrors = Serial.errors;
    check(!drawLineChartSeriesWidget(options, missingBuffer, 2) &&
          Serial.errors == previousErrors + 1 && graphics.rectangles.empty(),
          "missing series buffer rejected");
    graphics.reset();
    check(!drawLineChartSeriesWidget(options, nullptr, 1), "missing series array rejected");
    graphics.reset();
    check(drawLineChartSeriesWidget(options, nullptr, 0) &&
          graphics.rectangles.size() == 1 && graphics.lines.empty(),
          "zero series draws an empty chart");

    options = testOptions();
    const LineChartPoint bars[] = {{1, 2}, {3, 5}, {5, 0}, {7, NAN}, {9, 0.00001}};
    graphics.reset();
    check(drawBarChartWidget(options, bars, 5, 1.0), "draw daily bars");
    check(graphics.rectangles.size() == 4 && graphics.lines.empty() &&
          graphics.pixels.empty(), "three positive bars, no line or invented zero bar");
    check(graphics.rectangles[1].x == 17 && graphics.rectangles[1].width == 11 &&
          graphics.rectangles[1].y == 122 && graphics.rectangles[1].height == 20,
          "bar width uses X units and height uses zero baseline");
    check(graphics.rectangles[2].y == 92 && graphics.rectangles[2].height == 50 &&
          graphics.rectangles[3].height == 1,
          "bar heights proportional, tiny positive precipitation remains visible");
    const LineChartPoint clippedBars[] = {{0, 20}, {10, 5}};
    graphics.reset();
    check(drawBarChartWidget(options, clippedBars, 2, 2.0), "clip bars at plot edges");
    for (size_t i = 1; i < graphics.rectangles.size(); ++i)
    {
        const RecordedRect &bar = graphics.rectangles[i];
        check(bar.x >= 12 && bar.x + bar.width <= 113 &&
              bar.y >= 42 && bar.y + bar.height <= 142,
              "bar rectangles stay inside plot without covering baseline");
    }
    const LineChartPoint negativeBar[] = {{1, -0.1}};
    graphics.reset();
    previousErrors = Serial.errors;
    check(!drawBarChartWidget(options, negativeBar, 1, 1.0) &&
          Serial.errors == previousErrors + 1 && graphics.rectangles.empty(),
          "negative precipitation logged without partial drawing");
    for (double width : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::quiet_NaN()})
    {
        graphics.reset();
        previousErrors = Serial.errors;
        check(!drawBarChartWidget(options, bars, 5, width) &&
              Serial.errors == previousErrors + 1 && graphics.rectangles.empty(),
              "invalid bar width logged without partial drawing");
    }
    options.minimumY = -1;
    graphics.reset();
    check(!drawBarChartWidget(options, bars, 5, 1.0) && graphics.rectangles.empty(),
          "bar chart requires zero baseline");
    options = testOptions();
    options.x = 0;
    options.width = SCREEN_WIDTH;
    options.maximumX = 14;
    options.maximumY = 0.06;
    options.showYScale = true;
    options.yTickInterval = 0.02;
    options.yLabelInterval = 0.02;
    options.yLabelDecimalPlaces = 2;
    options.zeroBarColor = CYD_YELLOW;
    LineChartPoint dailyBars[14];
    for (int day = 0; day < 14; ++day)
        dailyBars[day] = {day + 0.5, day == 4 ? 0.047 : 0.0};
    graphics.reset();
    check(drawBarChartWidget(options, dailyBars, 14, 0.75) &&
          graphics.rectangles.size() == 2 && graphics.texts.size() == 4,
          "14-day export amounts draw one rain bar with fractional inch labels");
    graphics.reset();
    check(drawBarChartWidget(options, dailyBars, 14, 0.75, 3) &&
          graphics.rectangles.size() == 15,
          "each of 14 dates has a bar including zero markers");
    for (size_t day = 0; day < 14; ++day)
    {
        const RecordedRect &bar = graphics.rectangles[day + 1];
        check(bar.width > 0, "daily bars have positive width");
        if (day != 4)
            check(bar.height == 3 && bar.color == CYD_YELLOW &&
                  bar.color != options.gridColor,
                  "zero precipitation draws exactly 3 yellow pixels distinct from grid");
        else
            check(bar.height > 3 && bar.color == options.lineColor,
                  "rain bar keeps proportional height and rain color");
        if (day > 0)
        {
            const RecordedRect &previous = graphics.rectangles[day];
            check(bar.x > previous.x + previous.width,
                  "visible gap separates every adjacent daily bar");
        }
    }
    graphics.reset();
    check(drawBarChartWidget(options, bars, 5, 0.75, 3) &&
          graphics.rectangles.size() == 5,
          "zero markers do not turn NaN into a dry day");
    graphics.reset();
    previousErrors = Serial.errors;
    check(!drawBarChartWidget(options, dailyBars, 14, 0.75, -1) &&
          Serial.errors == previousErrors + 1 && graphics.rectangles.empty(),
          "negative zero marker height rejected before drawing");
    for (LineChartPoint &bar : dailyBars)
        bar.y = 0.0;
    graphics.reset();
    check(drawBarChartWidget(options, dailyBars, 14, 0.75) &&
          graphics.rectangles.size() == 1 && !graphics.texts.empty(),
          "all dry or missing dates draw zero-height bars and valid axes");
    graphics.reset();
    check(drawBarChartWidget(options, dailyBars, 14, 0.75, 3) &&
          graphics.rectangles.size() == 15,
          "all dry dates remain countable");
    for (size_t i = 1; i < graphics.rectangles.size(); ++i)
        check(graphics.rectangles[i].height == 3 &&
              graphics.rectangles[i].color == CYD_YELLOW,
              "all-dry chart uses 3-pixel yellow markers");
    graphics.reset();
    check(drawBarChartWidget(options, dailyBars, 14, 0.75, 1000),
          "oversized zero markers clipped to plot");
    for (size_t i = 1; i < graphics.rectangles.size(); ++i)
        check(graphics.rectangles[i].y == options.y + 4 &&
              graphics.rectangles[i].height == options.height - 9,
              "zero markers cannot extend outside plot");

    options = testOptions();
    options.minimumY = -10;
    const LineChartPoint lows[] = {{1, -5}, {3, -8}, {5, 4}, {7, NAN}, {9, 0}};
    const LineChartPoint highs[] = {{1, 5}, {3, -2}, {5, 4}, {7, 6}, {9, NAN}};
    graphics.reset();
    check(drawFloatingBarChartWidget(options, lows, highs, 5, 1.0),
          "floating bars accept negative temperatures and missing pairs");
    check(graphics.rectangles.size() == 4 && graphics.lines.empty() &&
          graphics.pixels.empty(), "only three paired dates draw floating bars");
    check(graphics.rectangles[1].x == 17 && graphics.rectangles[1].width == 11 &&
          graphics.rectangles[1].y == 67 && graphics.rectangles[1].height == 51,
          "floating bar spans exact daily low to high, not a zero baseline");
    check(graphics.rectangles[2].y == 102 && graphics.rectangles[2].height == 31,
          "entirely negative daily range remains floating");
    check(graphics.rectangles[3].y == 72 && graphics.rectangles[3].height == 1,
          "equal daily high and low remain visible");
    const LineChartPoint edgeLows[] = {{-2, 0}, {0, -20}, {5, 12}, {10, -5}, {12, 0}};
    const LineChartPoint edgeHighs[] = {{-2, 4}, {0, 20}, {5, 15}, {10, 15}, {12, 4}};
    graphics.reset();
    check(drawFloatingBarChartWidget(options, edgeLows, edgeHighs, 5, 2.0) &&
          graphics.rectangles.size() == 3, "floating bars clip or omit offscreen ranges");
    for (size_t i = 1; i < graphics.rectangles.size(); ++i)
    {
        const RecordedRect &bar = graphics.rectangles[i];
        check(bar.x >= 12 && bar.x + bar.width <= 113 &&
              bar.y >= 42 && bar.y + bar.height <= 143,
              "floating rectangles stay within the plot");
    }
    const LineChartPoint badLower[] = {{1, 8}};
    const LineChartPoint badUpper[] = {{1, 2}};
    graphics.reset();
    previousErrors = Serial.errors;
    check(!drawFloatingBarChartWidget(options, badLower, badUpper, 1, 1.0) &&
          Serial.errors == previousErrors + 1 && graphics.rectangles.empty(),
          "inverted temperature range logged before drawing");
    const LineChartPoint wrongDate[] = {{2, 9}};
    graphics.reset();
    previousErrors = Serial.errors;
    check(!drawFloatingBarChartWidget(options, badLower, wrongDate, 1, 1.0) &&
          Serial.errors == previousErrors + 1 && graphics.rectangles.empty(),
          "mismatched daily dates rejected before drawing");
    graphics.reset();
    check(drawFloatingBarChartWidget(options, nullptr, nullptr, 0, 1.0) &&
          graphics.rectangles.size() == 1, "empty floating chart has no invented bars");
    graphics.reset();
    previousErrors = Serial.errors;
    check(!drawFloatingBarChartWidget(options, nullptr, highs, 5, 1.0) &&
          Serial.errors == previousErrors + 1 && graphics.rectangles.empty(),
          "missing floating endpoint buffer rejected before drawing");
    for (double width : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::quiet_NaN()})
    {
        graphics.reset();
        previousErrors = Serial.errors;
        check(!drawFloatingBarChartWidget(options, lows, highs, 5, width) &&
              Serial.errors == previousErrors + 1 && graphics.rectangles.empty(),
              "invalid floating bar width rejected before drawing");
    }
    options = testOptions();
    options.x = 0;
    options.width = SCREEN_WIDTH;
    options.maximumX = 14;
    options.minimumY = -20;
    options.maximumY = 80;
    options.showYScale = true;
    options.yTickInterval = 5;
    options.yLabelInterval = 10;
    LineChartPoint dailyLows[14];
    LineChartPoint dailyHighs[14];
    for (size_t day = 0; day < 14; ++day)
    {
        dailyLows[day] = {day + 0.5, -10.0 + day};
        dailyHighs[day] = {day + 0.5, 50.0 + day};
    }
    graphics.reset();
    check(drawFloatingBarChartWidget(options, dailyLows, dailyHighs, 14, 0.75) &&
          graphics.rectangles.size() == 15 && !graphics.texts.empty(),
          "14 paired dates draw 14 floating bars with a signed Fahrenheit scale");
    for (size_t day = 1; day < 14; ++day)
    {
        const RecordedRect &previous = graphics.rectangles[day];
        const RecordedRect &bar = graphics.rectangles[day + 1];
        check(bar.x > previous.x + previous.width,
              "75-percent daily width leaves a gap between floating bars");
    }
    for (LineChartPoint &low : dailyLows)
        low.y = NAN;
    graphics.reset();
    check(drawFloatingBarChartWidget(options, dailyLows, dailyHighs, 14, 0.75) &&
          graphics.rectangles.size() == 1,
          "all unpaired daily highs draw no invented ranges");

    options = testOptions();
    options.minimumY = 1018;
    options.maximumY = 1024;
    const LineChartPoint trendLows[] = {
        {1, 1020}, {3, 1020}, {5, 1020}, {7, 1020}, {9, 1020},
    };
    const LineChartPoint trendHighs[] = {
        {1, 1020}, {3, 1020.8}, {5, 1020.80001}, {7, 1021.5}, {9, 1021.50001},
    };
    const LineChartPoint trendAverages[] = {
        {1, 1020}, {3, 1020.2}, {5, 1020.6}, {7, 1021.1}, {9, 1021.2},
    };
    graphics.reset();
    check(drawPressureTrendChartWidget(options, trendLows, trendHighs, trendAverages, 5, 0.75),
          "pressure bars accept exact thresholds and supplied averages");
    const uint16_t expectedColors[] = {CYD_GREEN, CYD_GREEN, CYD_YELLOW, CYD_YELLOW, CYD_ORANGE};
    check(graphics.rectangles.size() == 6 && graphics.lines.size() == 5,
          "each pressure bucket draws one high-low bar and one average tick");
    for (size_t i = 0; i < 5; ++i)
    {
        const auto &bar = graphics.rectangles[i + 1];
        const auto &tick = graphics.lines[i];
        const int expectedY = 42 + static_cast<int>(
            std::floor((1024 - trendAverages[i].y) / 6 * 100 + 0.5));
        check(bar.color == expectedColors[i], "strict pressure range color thresholds");
        check(tick.y0 == expectedY && tick.y1 == expectedY &&
              tick.x0 == bar.x - 1 && tick.x1 == bar.x + bar.width &&
              tick.color == options.labelColor,
              "contrasting average tick uses the supplied average, not range midpoint");
    }
    check(graphics.rectangles[1].height == 1, "equal pressure min/max stays visible");
    const LineChartPoint invalidAverages[] = {{1, 1021}};
    graphics.reset();
    previousErrors = Serial.errors;
    check(!drawPressureTrendChartWidget(options, trendLows, trendHighs, invalidAverages, 1, 0.75) &&
          Serial.errors == previousErrors + 1 && graphics.rectangles.empty(),
          "average outside range rejected before clearing chart");
    graphics.reset();
    check(!drawPressureTrendChartWidget(options, trendLows, trendHighs, nullptr, 5, 0.75) &&
          graphics.rectangles.empty(), "missing average buffer rejected");
    graphics.reset();
    check(drawPressureTrendChartWidget(options, nullptr, nullptr, nullptr, 0, 0.75) &&
          graphics.rectangles.size() == 1, "empty pressure chart does not invent bars");

    options = testOptions();
    options.maximumX = 3;
    options.maximumY = 100;
    options.lineColor = CYD_YELLOW;
    LineChartPoint sunshineBars[] = {{0.5, 0}, {1.5, 50}, {2.5, 100}};
    LineChartPoint meanClouds[] = {{0.5, 100}, {1.5, 0}, {2.5, 75}};
    graphics.reset();
    check(drawBarLineChartWidget(options, sunshineBars, meanClouds, 3, 1, CYD_RED) &&
          graphics.rectangles.size() == 3 && graphics.lines.size() == 2 &&
          graphics.pixels.size() == 3,
          "yellow sunshine bars and independent red cloud line share one background clear");
    check(graphics.rectangles[1].color == CYD_YELLOW &&
          graphics.rectangles[1].y == 92 && graphics.rectangles[1].height == 50 &&
          graphics.rectangles[2].color == CYD_YELLOW &&
          graphics.rectangles[2].y == 42 && graphics.rectangles[2].height == 100,
          "sunshine bars rise from zero to exact percentages; zero has no fabricated height");
    for (const auto &line : graphics.lines)
        check(line.color == CYD_RED, "mean cloud cover is a red line, not another bar");
    check(graphics.rectangles[2].x <=
          graphics.rectangles[1].x + graphics.rectangles[1].width,
          "full-slot sunshine bars have no pixel gap between nonzero dates");
    sunshineBars[1].y = NAN;
    graphics.reset();
    check(drawBarLineChartWidget(options, sunshineBars, meanClouds, 3, 1, CYD_RED) &&
          graphics.rectangles.size() == 2 && graphics.lines.size() == 2,
          "missing sunshine does not omit the mean cloud cover line");
    meanClouds[1].y = NAN;
    graphics.reset();
    check(drawBarLineChartWidget(options, sunshineBars, meanClouds, 3, 1, CYD_RED) &&
          graphics.rectangles.size() == 2 && graphics.lines.empty() &&
          graphics.pixels.size() == 2,
          "null sunshine omits a bar and null mean cover breaks the line independently");
    sunshineBars[1].y = 50;
    graphics.reset();
    check(drawBarLineChartWidget(options, sunshineBars, meanClouds, 3, 1, CYD_RED) &&
          graphics.rectangles.size() == 3 && graphics.lines.empty(),
          "missing mean cloud cover does not omit the sunshine bar");
    meanClouds[1].y = 0;
    meanClouds[1].x = 1.6;
    graphics.reset();
    check(!drawBarLineChartWidget(options, sunshineBars, meanClouds, 3, 1, CYD_RED) &&
          graphics.rectangles.empty(), "mismatched bar/line dates reject before clearing");
    meanClouds[1].x = 1.5;
    meanClouds[1].y = INFINITY;
    graphics.reset();
    check(!drawBarLineChartWidget(options, sunshineBars, meanClouds, 3, 1, CYD_RED) &&
          graphics.rectangles.empty(), "invalid cloud line rejects before drawing sunshine bars");
    meanClouds[1].y = 0;
    sunshineBars[1].y = -1;
    check(!drawBarLineChartWidget(options, sunshineBars, meanClouds, 3, 1, CYD_RED) &&
          graphics.rectangles.empty(), "negative sunshine bars reject before clearing");
    sunshineBars[1].y = 50;
    check(!drawBarLineChartWidget(options, sunshineBars, nullptr, 3, 1, CYD_RED) &&
          !drawBarLineChartWidget(options, sunshineBars, meanClouds, 3, 0, CYD_RED) &&
          graphics.rectangles.empty(), "missing line buffer and invalid bar width reject safely");
    options.minimumY = 1;
    check(!drawBarLineChartWidget(options, sunshineBars, meanClouds, 3, 1, CYD_RED) &&
          graphics.rectangles.empty(), "sunshine bars require a zero baseline");
    options.minimumY = 0;
    graphics.reset();
    check(drawBarLineChartWidget(options, nullptr, nullptr, 0, 1, CYD_RED) &&
          graphics.rectangles.size() == 1, "empty bar/line chart draws no invented values");
    options.pastForecastX = 1;
    graphics.reset();
    check(drawBarLineChartWidget(options, sunshineBars, meanClouds, 3, 1, CYD_RED) &&
          graphics.texts.size() == 2 && graphics.texts[0].value == "Past" &&
          graphics.texts[1].value == "Forecast" &&
          graphics.lines.back().color == options.labelColor,
          "bar/line chart draws today's white divider above both series");

    options = testOptions();
    options.maximumX = 28;
    options.maximumY = 100;
    options.lineColor = CYD_DARK_GRAY;
    LineChartPoint cloudLows[28], cloudHighs[28], sunshine[28];
    for (size_t i = 0; i < 28; ++i)
    {
        cloudLows[i] = {i + 0.5, 20};
        cloudHighs[i] = {i + 0.5, 60};
        sunshine[i] = {i + 0.5, i % 2 ? 0.0 : 100.0};
    }
    graphics.reset();
    check(drawFloatingBarLineChartWidget(options, cloudLows, cloudHighs, sunshine,
                                         28, 1.0, CYD_YELLOW) &&
          graphics.rectangles.size() == 29 && graphics.lines.size() == 27,
          "cloud range and independent sunshine line draw together with one background clear");
    for (size_t i = 0; i < 28; ++i)
    {
        const auto &bar = graphics.rectangles[i + 1];
        check(bar.color == CYD_DARK_GRAY && bar.y == 82 && bar.height == 41,
              "uniform gray bars map exact cloud-cover endpoints");
        if (i)
        {
            const auto &previous = graphics.rectangles[i];
            check(bar.x <= previous.x + previous.width,
                  "full-width daily bars have no pixel gaps across all 28 dates");
        }
    }
    for (const auto &line : graphics.lines)
        check(line.color == CYD_YELLOW &&
              ((line.y0 == 42 && line.y1 == 142) || (line.y0 == 142 && line.y1 == 42)),
              "yellow sunshine line may lie outside the cloud-cover range");
    cloudLows[0].y = NAN;
    sunshine[1].y = NAN;
    graphics.reset();
    check(drawFloatingBarLineChartWidget(options, cloudLows, cloudHighs, sunshine,
                                         28, 1.0, CYD_YELLOW) &&
          graphics.rectangles.size() == 28 && graphics.lines.size() == 25,
          "missing cloud range leaves sunshine independent and null sunshine breaks the line");
    sunshine[0].x = 0.75;
    graphics.reset();
    check(!drawFloatingBarLineChartWidget(options, cloudLows, cloudHighs, sunshine,
                                          28, 1.0, CYD_YELLOW) &&
          graphics.rectangles.empty(), "mismatched overlay dates reject before drawing");
    graphics.reset();
    check(!drawFloatingBarLineChartWidget(options, cloudLows, cloudHighs, nullptr,
                                          28, 1.0, CYD_YELLOW) &&
          graphics.rectangles.empty(), "missing overlay buffer rejects before drawing");
    check(!drawFloatingBarLineChartWidget(options, cloudLows, cloudHighs, sunshine,
                                          28, 0, CYD_YELLOW) &&
          graphics.rectangles.empty(), "invalid overlay width rejects before drawing");
    check(drawFloatingBarLineChartWidget(options, nullptr, nullptr, nullptr,
                                         0, 1.0, CYD_YELLOW),
          "empty range-line chart draws no invented values");
    options = testOptions();
    options.maximumX = 28;
    options.maximumY = 100;
    options.width = 305;
    options.lineColor = CYD_DARK_GRAY;
    options.pastForecastX = 10;
    sunshine[0].x = 0.5;
    graphics.reset();
    check(drawFloatingBarLineChartWidget(options, cloudLows, cloudHighs, sunshine,
                                         28, 1, CYD_YELLOW),
          "Past/Forecast divider draws with range and sunshine overlay");
    const int dividerX = 12 + static_cast<int>(std::floor(10.0 / 28 * 300 + 0.5));
    size_t dashCount = 0;
    for (const auto &line : graphics.lines)
        if (line.color == options.labelColor)
        {
            check(line.x0 == dividerX && line.x1 == dividerX &&
                  line.y0 == 54 + static_cast<int>(dashCount) * 7 &&
                  line.y1 == std::min(line.y0 + 3, 142),
                  "white divider follows date X, not midpoint, with clipped four-pixel dashes");
            ++dashCount;
        }
    check(dashCount == 13 && graphics.lines.back().color == options.labelColor,
          "divider spans plot and is drawn above bars and sunshine");
    check(graphics.texts.size() == 2 &&
          graphics.texts[0].value == "Past" &&
          graphics.texts[0].x == dividerX - 28 &&
          graphics.texts[1].value == "Forecast" &&
          graphics.texts[1].x == dividerX + 4 &&
          graphics.texts[0].y == 42 && graphics.texts[1].y == 42 &&
          graphics.texts[0].color == options.labelColor &&
          graphics.texts[1].color == options.labelColor,
          "white labels sit on the appropriate sides in a separate row above the data");
    graphics.reset();
    check(drawFloatingBarChartWidget(options, cloudLows, cloudHighs, 28, 0.75) &&
          graphics.texts.size() == 2 && graphics.texts[0].value == "Past" &&
          graphics.texts[1].value == "Forecast" && graphics.lines.size() == dashCount,
          "temperature-style floating bars also draw the dashed divider and both labels");
    for (const auto &line : graphics.lines)
        check(line.x0 == dividerX && line.x1 == dividerX && line.color == options.labelColor,
              "floating bar divider is white and uses today's date coordinate");
    const LineChartPoint dailySpeed[] = {{0.5, 10}, {27.5, 20}};
    const LineChartPoint dailyGust[] = {{0.5, 20}, {27.5, 30}};
    const LineChartSeries dailyLines[] = {
        {dailySpeed, 2, CYD_CYAN}, {dailyGust, 2, CYD_YELLOW},
    };
    graphics.reset();
    check(drawLineChartSeriesWidget(options, dailyLines, 2) &&
          graphics.texts.size() == 2 && graphics.texts[0].value == "Past" &&
          graphics.texts[1].value == "Forecast" &&
          graphics.lines.size() == dashCount + 2 &&
          graphics.lines[0].color == CYD_CYAN && graphics.lines[1].color == CYD_YELLOW &&
          graphics.lines.back().color == options.labelColor,
          "daily lines retain their colors with the divider drawn above them");
    options.forecastBarInterval = 1;
    const LineChartPoint temperatureLows[] = {{9.5, 10}, {10.5, 20}, {11.5, 30}};
    const LineChartPoint temperatureHighs[] = {{9.5, 40}, {10.5, 50}, {11.5, 60}};
    graphics.reset();
    check(drawFloatingBarChartWidget(options, temperatureLows, temperatureHighs, 3, 0.75) &&
          graphics.rectangles.size() == 4 &&
          graphics.rectangles[1].color == options.lineColor &&
          graphics.rectangles[2].color == CYD_YELLOW &&
          graphics.rectangles[3].color == CYD_GREEN,
          "past bars retain their color, today's slot is yellow, later slots are green");
    options.pastForecastX = 11;
    graphics.reset();
    check(drawFloatingBarChartWidget(options, temperatureLows, temperatureHighs, 3, 0.75) &&
          graphics.rectangles[1].color == options.lineColor &&
          graphics.rectangles[2].color == options.lineColor &&
          graphics.rectangles[3].color == CYD_YELLOW,
          "bar colors move with the local date divider");
    options.pastForecastX = NAN;
    graphics.reset();
    check(drawFloatingBarChartWidget(options, temperatureLows, temperatureHighs, 3, 0.75) &&
          graphics.rectangles[1].color == options.lineColor &&
          graphics.rectangles[2].color == options.lineColor &&
          graphics.rectangles[3].color == options.lineColor,
          "unavailable local date leaves all bars in their base color");
    options.forecastBarInterval = -1;
    expectInvalid(options, diagonal, 2);
    options.forecastBarInterval = std::numeric_limits<double>::infinity();
    expectInvalid(options, diagonal, 2);
    options.forecastBarInterval = 0;
    options.pastForecastX = 0;
    graphics.reset();
    check(drawFloatingBarLineChartWidget(options, nullptr, nullptr, nullptr, 0, 1, CYD_YELLOW) &&
          graphics.texts.size() == 1 && graphics.texts[0].value == "Forecast",
          "at the first date, only Forecast fits and no label leaves the chart");
    options.pastForecastX = 28;
    graphics.reset();
    check(drawFloatingBarLineChartWidget(options, nullptr, nullptr, nullptr, 0, 1, CYD_YELLOW) &&
          graphics.texts.size() == 1 && graphics.texts[0].value == "Past",
          "at the end date, only Past fits");
    for (double outside : {-1.0, 29.0})
    {
        options.pastForecastX = outside;
        graphics.reset();
        check(drawFloatingBarLineChartWidget(options, nullptr, nullptr, nullptr, 0, 1, CYD_YELLOW) &&
              graphics.lines.empty() && graphics.texts.empty(),
              "divider outside supplied dates is omitted rather than clamped to an edge");
    }
    options.pastForecastX = std::numeric_limits<double>::infinity();
    expectInvalid(options, diagonal, 2);
    options.pastForecastX = 10;
    options.height = 15;
    expectInvalid(options, diagonal, 2);
    std::printf("PASS: %zu line-chart checks\n", checks);
    return 0;
}
