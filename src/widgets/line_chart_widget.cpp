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
      labelColor(COLOR_TEXT),
      zeroBarColor(CYD_DARK_GRAY),
      pastForecastX(NAN),
      forecastBarInterval(0.0),
      referenceLineY(NAN),
      referenceLineColor(CYD_RED)
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

static void drawBars(const LineChartWidgetOptions &options,
                     const LineChartSeries &series, double barWidth,
                     int zeroBarHeight,
                     double rangeX, double rangeY,
                     int left, int top, int right, int bottom)
{
    const double halfWidth = barWidth / rangeX / 2.0;
    for (size_t i = 0; i < series.pointCount; ++i)
    {
        const LineChartPoint &point = series.points[i];
        if (isnan(point.y) || (point.y == 0.0 && zeroBarHeight == 0))
            continue;
        const double center = (point.x - options.minimumX) / rangeX;
        if (center + halfWidth < 0.0 || center - halfWidth > 1.0)
            continue;
        const int x0 = chartPixel(center - halfWidth, left, right);
        const int x1 = chartPixel(center + halfWidth, left, right);
        const int y = point.y == 0.0
            ? bottom - min(zeroBarHeight, bottom - top)
            : min(bottom - 1, chartPixel(1.0 - point.y / rangeY, top, bottom));
        gfx->fillRect(x0, y, x1 - x0 + 1, bottom - y,
                      point.y == 0.0 ? options.zeroBarColor : series.color);
    }
}

static void drawPastForecastDivider(const LineChartWidgetOptions &options,
                                    int left, int top, int right, int bottom)
{
    if (isnan(options.pastForecastX) ||
        options.pastForecastX < options.minimumX ||
        options.pastForecastX > options.maximumX)
        return;
    const int x = chartPixel(
        (options.pastForecastX - options.minimumX) /
            (options.maximumX - options.minimumX), left, right);
    for (int y = top; y <= bottom; y += 7)
        gfx->drawLine(x, y, x, min(y + 3, bottom), options.labelColor);
    setDefaultFont();
    gfx->setTextSize(1);
    gfx->setTextColor(options.labelColor);
    if (x - left >= 28)
    {
        gfx->setCursor(x - 28, top - 12);
        gfx->print("Past");
    }
    if (right - x >= 52)
    {
        gfx->setCursor(x + 4, top - 12);
        gfx->print("Forecast");
    }
    gfx->setTextColor(COLOR_TEXT);
    gfx->setTextSize(2);
}

static void drawReferenceLine(const LineChartWidgetOptions &options,
                              int left, int top, int right, int bottom)
{
    if (isnan(options.referenceLineY) ||
        options.referenceLineY < options.minimumY ||
        options.referenceLineY > options.maximumY)
        return;
    const int y = chartPixel((options.maximumY - options.referenceLineY) /
                             (options.maximumY - options.minimumY), top, bottom);
    for (int x = left; x <= right; x += 7)
        gfx->drawLine(x, y, min(x + 3, right), y, options.referenceLineColor);
}

static bool drawChartWidget(const LineChartWidgetOptions &options,
                             const LineChartSeries *series, size_t seriesCount,
                             double barWidth, int zeroBarHeight,
                             bool floatingBars = false, bool rangeLine = false,
                             bool barLine = false, bool precipitationBars = false)
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
        isinf(options.pastForecastX) || isinf(options.referenceLineY) ||
        !isfinite(options.forecastBarInterval) || options.forecastBarInterval < 0 ||
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
        for (size_t i = 0; barWidth > 0.0 && !floatingBars && (!barLine || s == 0) &&
             i < series[s].pointCount; ++i)
        {
            if (series[s].points[i].y < 0.0)
            {
                Serial.println("Bar chart widget: negative bar value.");
                return false;
            }
        }
    }

    if (barLine)
        for (size_t i = 0; i < series[0].pointCount; ++i)
            if (series[0].points[i].x != series[1].points[i].x)
            {
                Serial.println("Bar line chart widget: mismatched X.");
                return false;
            }

    if (precipitationBars)
        for (size_t i = 0; i < series[0].pointCount; ++i)
        {
            const auto &rain = series[0].points[i];
            const auto &showers = series[1].points[i];
            const auto &hours = series[2].points[i];
            if (rain.x != showers.x || rain.x != hours.x ||
                (!isnan(hours.y) && (hours.y < 0 || hours.y > 24)) ||
                (isnan(rain.y) != isnan(showers.y)) ||
                (!isnan(rain.y) && (isnan(hours.y) || rain.y < 0 ||
                  rain.y > showers.y || showers.y > hours.y)))
            {
                Serial.println("Precipitation chart widget: invalid or mismatched values.");
                return false;
            }
        }

    if (floatingBars)
    {
        for (size_t i = 0; i < series[0].pointCount; ++i)
        {
            const LineChartPoint &lower = series[0].points[i];
            const LineChartPoint &upper = series[1].points[i];
            if (lower.x != upper.x || lower.y > upper.y)
            {
                Serial.println("Floating bar chart widget: mismatched X or inverted range.");
                return false;
            }
            if (rangeLine && series[2].points[i].x != lower.x)
            {
                Serial.println("Floating bar line chart widget: mismatched X.");
                return false;
            }
            if (seriesCount == 3 && !rangeLine)
            {
                const LineChartPoint &average = series[2].points[i];
                if (average.x != lower.x || isnan(lower.y) || isnan(upper.y) ||
                    isnan(average.y) || average.y < lower.y || average.y > upper.y)
                {
                    Serial.println("Pressure trend chart widget: invalid average.");
                    return false;
                }
            }
        }
    }

    int left = options.x + 2;
    const int top = options.y + (options.showYScale ? 4 : 2) +
        (isnan(options.pastForecastX) ? 0 : 12);
    const int right = options.x + options.width - 3;
    const int bottom = options.y + options.height - (options.showYScale ? 5 : 3);
    if (!isnan(options.pastForecastX) && bottom - top < 7)
    {
        Serial.println("Line chart widget: bounds are too small for the Past/Forecast divider.");
        return false;
    }
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

    if (precipitationBars)
    {
        const double halfWidth = barWidth / rangeX / 2.0;
        for (size_t i = 0; i < series[0].pointCount; ++i)
        {
            const auto &rain = series[0].points[i];
            const auto &showers = series[1].points[i];
            const auto &hours = series[2].points[i];
            if (isnan(hours.y))
                continue;
            const double center = (hours.x - options.minimumX) / rangeX;
            if (center + halfWidth < 0 || center - halfWidth > 1)
                continue;
            const int x0 = chartPixel(center - halfWidth, left, right);
            const int x1 = chartPixel(center + halfWidth, left, right);
            uint16_t color = CYD_CYAN;
            if (!isnan(options.pastForecastX) && hours.x >= options.pastForecastX)
                color = hours.x - options.pastForecastX < options.forecastBarInterval
                    ? CYD_YELLOW : CYD_GREEN;
            gfx->fillRect(x0, top, x1 - x0 + 1, bottom - top, color);
            const int wetY = chartPixel(1.0 - hours.y / 24.0, top, bottom);
            if (isnan(rain.y))
            {
                if (wetY < bottom)
                    gfx->fillRect(x0, wetY, x1 - x0 + 1, bottom - wetY, CYD_DARK_GRAY);
                continue;
            }
            const int rainY = chartPixel(1.0 - rain.y / 24.0, top, bottom);
            const int showersY = chartPixel(1.0 - showers.y / 24.0, top, bottom);
            if (rainY < bottom)
                gfx->fillRect(x0, rainY, x1 - x0 + 1, bottom - rainY, CYD_GRAY);
            if (showersY < rainY)
                gfx->fillRect(x0, showersY, x1 - x0 + 1, rainY - showersY, CYD_MAGENTA);
            if (wetY < showersY)
                gfx->fillRect(x0, wetY, x1 - x0 + 1, showersY - wetY, CYD_WHITE);
        }
        drawReferenceLine(options, left, top, right, bottom);
        drawPastForecastDivider(options, left, top, right, bottom);
        return true;
    }
    if (floatingBars)
    {
        const double halfWidth = barWidth / rangeX / 2.0;
        for (size_t i = 0; i < series[0].pointCount; ++i)
        {
            const LineChartPoint &lower = series[0].points[i];
            const LineChartPoint &upper = series[1].points[i];
            if (isnan(lower.y) || isnan(upper.y) ||
                upper.y < options.minimumY || lower.y > options.maximumY)
                continue;
            const double center = (lower.x - options.minimumX) / rangeX;
            if (center + halfWidth < 0.0 || center - halfWidth > 1.0)
                continue;
            const int x0 = chartPixel(center - halfWidth, left, right);
            const int x1 = chartPixel(center + halfWidth, left, right);
            const int y0 = chartPixel((options.maximumY - upper.y) / rangeY,
                                     top, bottom);
            const int y1 = chartPixel((options.maximumY - lower.y) / rangeY,
                                     top, bottom);
            uint16_t color = options.lineColor;
            if (options.forecastBarInterval > 0 && lower.x >= options.pastForecastX)
                color = lower.x - options.pastForecastX < options.forecastBarInterval
                    ? CYD_YELLOW : CYD_GREEN;
            if (seriesCount == 3 && !rangeLine)
            {
                color = upper.y > lower.y + 1.5 ? CYD_ORANGE :
                    upper.y > lower.y + 0.8 ? CYD_YELLOW : CYD_GREEN;
            }
            gfx->fillRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1, color);
            if (seriesCount == 3 && !rangeLine)
            {
                const double average = series[2].points[i].y;
                if (average >= options.minimumY && average <= options.maximumY)
                {
                    const int y = chartPixel((options.maximumY - average) / rangeY,
                                             top, bottom);
                    gfx->drawLine(max(left, x0 - 1), y, min(right, x1 + 1), y,
                                  options.labelColor);
                }
            }
        }
        if (rangeLine)
            drawSeries(options, series[2], rangeX, rangeY, left, top, right, bottom);
        drawReferenceLine(options, left, top, right, bottom);
        drawPastForecastDivider(options, left, top, right, bottom);
        return true;
    }
    for (size_t s = 0; s < seriesCount; ++s)
    {
        if (barWidth > 0.0 && (!barLine || s == 0))
            drawBars(options, series[s], barWidth, zeroBarHeight, rangeX, rangeY,
                     left, top, right, bottom);
        else
            drawSeries(options, series[s], rangeX, rangeY, left, top, right, bottom);
    }
    drawReferenceLine(options, left, top, right, bottom);
    drawPastForecastDivider(options, left, top, right, bottom);
    return true;
}

bool drawLineChartSeriesWidget(const LineChartWidgetOptions &options,
                               const LineChartSeries *series, size_t seriesCount)
{
    return drawChartWidget(options, series, seriesCount, 0.0, 0);
}

bool drawBarChartWidget(const LineChartWidgetOptions &options,
                        const LineChartPoint *points, size_t pointCount,
                        double barWidth, int zeroBarHeight)
{
    if (!isfinite(barWidth) || barWidth <= 0.0 || zeroBarHeight < 0 ||
        options.minimumY != 0.0 ||
        !isfinite(barWidth / (options.maximumX - options.minimumX)))
    {
        Serial.println("Bar chart widget: invalid bar width, zero marker, or baseline.");
        return false;
    }
    const LineChartSeries series = {points, pointCount, options.lineColor};
    return drawChartWidget(options, &series, 1, barWidth, zeroBarHeight);
}

bool drawFloatingBarChartWidget(const LineChartWidgetOptions &options,
                                const LineChartPoint *lower,
                                const LineChartPoint *upper, size_t pointCount,
                                double barWidth)
{
    if (!isfinite(barWidth) || barWidth <= 0.0 ||
        !isfinite(barWidth / (options.maximumX - options.minimumX)))
    {
        Serial.println("Floating bar chart widget: invalid bar width.");
        return false;
    }
    const LineChartSeries series[] = {
        {lower, pointCount, options.lineColor},
        {upper, pointCount, options.lineColor},
    };
    return drawChartWidget(options, series, 2, barWidth, 0, true);
}

bool drawBarLineChartWidget(const LineChartWidgetOptions &options,
                            const LineChartPoint *bars, const LineChartPoint *line,
                            size_t pointCount, double barWidth, uint16_t lineColor)
{
    if (!isfinite(barWidth) || barWidth <= 0.0 || options.minimumY != 0.0 ||
        !isfinite(barWidth / (options.maximumX - options.minimumX)))
    {
        Serial.println("Bar line chart widget: invalid bar width or baseline.");
        return false;
    }
    const LineChartSeries series[] = {
        {bars, pointCount, options.lineColor},
        {line, pointCount, lineColor},
    };
    return drawChartWidget(options, series, 2, barWidth, 0, false, false, true);
}

bool drawPrecipitationChartWidget(const LineChartWidgetOptions &options,
                                  const LineChartPoint *rain,
                                  const LineChartPoint *showers,
                                  const LineChartPoint *hours, size_t pointCount,
                                  double barWidth)
{
    if (!isfinite(barWidth) || barWidth <= 0 ||
        options.minimumY != 0 || options.maximumY != 24 ||
        !isfinite(barWidth / (options.maximumX - options.minimumX)))
    {
        Serial.println("Precipitation chart widget: invalid width or scale.");
        return false;
    }
    const LineChartSeries series[] = {
        {rain, pointCount, CYD_GRAY},
        {showers, pointCount, CYD_MAGENTA},
        {hours, pointCount, CYD_WHITE},
    };
    return drawChartWidget(options, series, 3, barWidth, 0, false, false, false, true);
}

bool drawPressureTrendChartWidget(const LineChartWidgetOptions &options,
                                 const LineChartPoint *lower,
                                 const LineChartPoint *upper,
                                 const LineChartPoint *average, size_t pointCount,
                                 double barWidth)
{
    if (!isfinite(barWidth) || barWidth <= 0.0 ||
        !isfinite(barWidth / (options.maximumX - options.minimumX)))
    {
        Serial.println("Pressure trend chart widget: invalid bar width.");
        return false;
    }
    const LineChartSeries series[] = {
        {lower, pointCount, options.lineColor},
        {upper, pointCount, options.lineColor},
        {average, pointCount, options.labelColor},
    };
    return drawChartWidget(options, series, 3, barWidth, 0, true);
}

bool drawFloatingBarLineChartWidget(const LineChartWidgetOptions &options,
                                    const LineChartPoint *lower,
                                    const LineChartPoint *upper,
                                    const LineChartPoint *line, size_t pointCount,
                                    double barWidth, uint16_t lineColor)
{
    if (!isfinite(barWidth) || barWidth <= 0.0 ||
        !isfinite(barWidth / (options.maximumX - options.minimumX)))
    {
        Serial.println("Floating bar line chart widget: invalid bar width.");
        return false;
    }
    const LineChartSeries series[] = {
        {lower, pointCount, options.lineColor},
        {upper, pointCount, options.lineColor},
        {line, pointCount, lineColor},
    };
    return drawChartWidget(options, series, 3, barWidth, 0, true, true);
}
