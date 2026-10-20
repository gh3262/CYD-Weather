#include "line_chart_widget.h"

#include <math.h>
#include <stdio.h>

#include "app.h"

LineChartWidgetOptions::LineChartWidgetOptions()
    : x(0),
      y(HEADER_HEIGHT),
      width(SCREEN_WIDTH),
      height(NAV_Y - HEADER_HEIGHT),
      minimumX(0.0),
      maximumX(100.0),
      minimumY(0.0),
      maximumY(100.0),
      showAxes(true),
      showGrid(true),
      showPointMarkers(false),
      showYScale(false),
      yTickInterval(5.0),
      yLabelInterval(10.0),
      yLabelDecimalPlaces(0),
      backgroundColor(COLOR_BACKGROUND),
      lineColor(CYD_CYAN),
      axisColor(COLOR_TEXT),
      gridColor(CYD_DARK_GRAY),
      labelColor(COLOR_TEXT)
{
}

struct ScaleSteps
{
    double first;
    size_t count;
};

static bool scaleSteps(double minimum, double maximum, double interval,
                       int maximumCount, ScaleSteps &steps)
{
    if (!isfinite(interval) || interval <= 0.0)
        return false;
    const double first = ceil(minimum / interval);
    const double last = floor(maximum / interval);
    const double count = fmax(0.0, last - first + 1.0);
    if (!isfinite(first) || !isfinite(last) ||
        fabs(first) > 9007199254740991.0 ||
        fabs(last) > 9007199254740991.0 ||
        count > maximumCount)
        return false;
    steps = {first, static_cast<size_t>(count)};
    return true;
}

static int scaleLabel(char *buffer, size_t size, double value,
                      unsigned int decimalPlaces)
{
    if (fabs(value) < 0.5 * pow(10.0, -static_cast<int>(decimalPlaces)))
        value = 0.0;
    return snprintf(buffer, size, "%.*f", static_cast<int>(decimalPlaces), value);
}

// Liang-Barsky clipping preserves the slope of segments crossing the plot edge.
static bool clipBoundary(double direction, double distance,
                         double &start, double &end)
{
    if (direction == 0.0)
        return distance >= 0.0;

    const double ratio = distance / direction;
    if (direction < 0.0)
    {
        if (ratio > end)
            return false;
        if (ratio > start)
            start = ratio;
    }
    else
    {
        if (ratio < start)
            return false;
        if (ratio < end)
            end = ratio;
    }
    return true;
}

static int chartPixel(double fraction, int first, int last)
{
    // Floating-point clipping can leave an endpoint just outside the edge.
    fraction = fmax(0.0, fmin(1.0, fraction));
    return first + static_cast<int>(floor(fraction * (last - first) + 0.5));
}

static void drawChartSegment(double x0, double y0, double x1, double y1,
                             int left, int top, int right, int bottom,
                             uint16_t color)
{
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    double start = 0.0;
    double end = 1.0;
    if (!clipBoundary(-dx, x0, start, end) ||
        !clipBoundary(dx, 1.0 - x0, start, end) ||
        !clipBoundary(-dy, y0, start, end) ||
        !clipBoundary(dy, 1.0 - y0, start, end))
        return;

    gfx->drawLine(
        chartPixel(x0 + start * dx, left, right),
        chartPixel(1.0 - (y0 + start * dy), top, bottom),
        chartPixel(x0 + end * dx, left, right),
        chartPixel(1.0 - (y0 + end * dy), top, bottom),
        color);
}

bool drawLineChartWidget(const LineChartWidgetOptions &options,
                         const LineChartPoint *points, size_t pointCount)
{
    const LineChartSeries series = {points, pointCount, options.lineColor};
    return drawLineChartSeriesWidget(options, &series, 1);
}

static bool validSeriesPoints(const LineChartWidgetOptions &options,
                              const LineChartPoint *points, size_t pointCount,
                              double rangeX, double rangeY)
{
    for (size_t i = 0; i < pointCount; ++i)
    {
        if (!isfinite(points[i].x) || isinf(points[i].y) ||
            (i > 0 && points[i].x < points[i - 1].x) ||
            !isfinite((points[i].x - options.minimumX) / rangeX) ||
            (!isnan(points[i].y) &&
             !isfinite((points[i].y - options.minimumY) / rangeY)) ||
            (i > 0 &&
             (!isfinite((points[i].x - options.minimumX) / rangeX -
                        (points[i - 1].x - options.minimumX) / rangeX) ||
              (!isnan(points[i].y) && !isnan(points[i - 1].y) &&
               !isfinite((points[i].y - options.minimumY) / rangeY -
                         (points[i - 1].y - options.minimumY) / rangeY)))))
            return false;
    }
    return true;
}

static void drawSeries(const LineChartWidgetOptions &options,
                       const LineChartSeries &series, double rangeX, double rangeY,
                       int left, int top, int right, int bottom)
{
    const LineChartPoint *points = series.points;
    bool previousValid = false;
    double previousX = 0.0;
    double previousY = 0.0;
    for (size_t i = 0; i < series.pointCount; ++i)
    {
        if (isnan(points[i].y))
        {
            previousValid = false;
            continue;
        }

        const double x = (points[i].x - options.minimumX) / rangeX;
        const double y = (points[i].y - options.minimumY) / rangeY;
        if (previousValid)
        {
            drawChartSegment(previousX, previousY, x, y,
                             left, top, right, bottom, series.color);
        }
        if (x >= 0.0 && x <= 1.0 && y >= 0.0 && y <= 1.0)
        {
            const int pixelX = chartPixel(x, left, right);
            const int pixelY = chartPixel(1.0 - y, top, bottom);
            // A single reading (or a reading beside a gap) remains visible.
            gfx->drawPixel(pixelX, pixelY, series.color);
            if (options.showPointMarkers)
            {
                gfx->drawLine(max(left, pixelX - 1), pixelY,
                              min(right, pixelX + 1), pixelY, series.color);
                gfx->drawLine(pixelX, max(top, pixelY - 1),
                              pixelX, min(bottom, pixelY + 1), series.color);
            }
        }
        previousX = x;
        previousY = y;
        previousValid = true;
    }
}

bool drawLineChartSeriesWidget(const LineChartWidgetOptions &options,
                               const LineChartSeries *series, size_t seriesCount)
{
    const double rangeX = options.maximumX - options.minimumX;
    const double rangeY = options.maximumY - options.minimumY;
    bool validBuffers = seriesCount == 0 || series;
    for (size_t s = 0; validBuffers && s < seriesCount; ++s)
        validBuffers = series[s].pointCount == 0 || series[s].points;
    if (!gfx ||
        options.x < 0 || options.y < 0 ||
        options.width < 8 || options.height < 8 ||
        options.width > SCREEN_WIDTH || options.height > SCREEN_HEIGHT ||
        options.x > SCREEN_WIDTH - options.width ||
        options.y > SCREEN_HEIGHT - options.height ||
        !isfinite(options.minimumX) || !isfinite(options.maximumX) ||
        !isfinite(options.minimumY) || !isfinite(options.maximumY) ||
        !isfinite(rangeX) || !isfinite(rangeY) ||
        rangeX <= 0.0 || rangeY <= 0.0 ||
        !validBuffers)
    {
        Serial.println("Line chart widget: invalid bounds, range, or point buffer.");
        return false;
    }

    // Validate before clearing so a bad update leaves the previous chart intact.
    for (size_t s = 0; s < seriesCount; ++s)
    {
        if (!validSeriesPoints(options, series[s].points, series[s].pointCount,
                               rangeX, rangeY))
        {
            Serial.println("Line chart widget: invalid or unordered data point.");
            return false;
        }
    }

    int left = options.x + 2;
    const int top = options.y + (options.showYScale ? 4 : 2);
    const int right = options.x + options.width - 3;
    const int bottom = options.y + options.height - (options.showYScale ? 5 : 3);
    ScaleSteps ticks = {};
    ScaleSteps labels = {};
    if (options.showYScale)
    {
        if (options.yLabelDecimalPlaces > 3 ||
            !scaleSteps(options.minimumY, options.maximumY,
                        options.yTickInterval, bottom - top + 1, ticks) ||
            !scaleSteps(options.minimumY, options.maximumY,
                        options.yLabelInterval, (bottom - top) / 8 + 1, labels) ||
            rangeY / options.yLabelInterval > (bottom - top) / 8.0 ||
            rangeY / options.yTickInterval > bottom - top)
        {
            Serial.println("Line chart widget: invalid or overcrowded Y scale.");
            return false;
        }
        int labelWidth = 0;
        for (size_t i = 0; i < labels.count; ++i)
        {
            char label[48];
            const int length = scaleLabel(
                label, sizeof(label), (labels.first + i) * options.yLabelInterval,
                options.yLabelDecimalPlaces);
            if (length < 0 || static_cast<size_t>(length) >= sizeof(label))
            {
                Serial.println("Line chart widget: Y label is too wide.");
                return false;
            }
            labelWidth = max(labelWidth, length * 6);
        }
        left += labelWidth + 7;
        if (right - left < 3 || bottom - top < 7)
        {
            Serial.println("Line chart widget: bounds are too small for the Y scale.");
            return false;
        }
    }

    gfx->fillRect(options.x, options.y, options.width, options.height,
                  options.backgroundColor);
    if (options.showGrid)
    {
        for (int division = 1; division < 4; ++division)
        {
            const int gridX = chartPixel(division / 4.0, left, right);
            const int gridY = chartPixel(division / 4.0, top, bottom);
            gfx->drawLine(gridX, top, gridX, bottom, options.gridColor);
            gfx->drawLine(left, gridY, right, gridY, options.gridColor);
        }
    }
    if (options.showAxes || options.showYScale)
    {
        gfx->drawLine(left, top, left, bottom, options.axisColor);
    }
    if (options.showAxes)
    {
        gfx->drawLine(left, bottom, right, bottom, options.axisColor);
    }
    if (options.showYScale)
    {
        for (size_t i = 0; i < ticks.count; ++i)
        {
            const double value = (ticks.first + i) * options.yTickInterval;
            const int pixelY = chartPixel(
                (options.maximumY - value) / rangeY, top, bottom);
            gfx->drawLine(left - 3, pixelY, left, pixelY, options.axisColor);
        }
        setDefaultFont();
        gfx->setTextSize(1);
        gfx->setTextColor(options.labelColor);
        for (size_t i = 0; i < labels.count; ++i)
        {
            const double value = (labels.first + i) * options.yLabelInterval;
            const int pixelY = chartPixel(
                (options.maximumY - value) / rangeY, top, bottom);
            char label[48];
            const int length = scaleLabel(
                label, sizeof(label), value, options.yLabelDecimalPlaces);
            gfx->setCursor(left - 5 - length * 6, pixelY - 4);
            gfx->print(label);
        }
        gfx->setTextColor(COLOR_TEXT);
        gfx->setTextSize(2);
    }

    for (size_t s = 0; s < seriesCount; ++s)
        drawSeries(options, series[s], rangeX, rangeY, left, top, right, bottom);
    return true;
}
