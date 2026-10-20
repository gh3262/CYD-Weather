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

    std::printf("PASS: %zu line-chart checks\n", checks);
    return 0;
}
