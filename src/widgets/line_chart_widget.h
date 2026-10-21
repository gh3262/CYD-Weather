#ifndef LINE_CHART_WIDGET_H
#define LINE_CHART_WIDGET_H

#include <Arduino.h>

struct LineChartPoint
{
    double x;
    // NaN represents a missing reading and breaks the line.
    double y;
};

struct LineChartWidgetOptions
{
    int x;
    int y;
    int width;
    int height;
    double minimumX;
    double maximumX;
    double minimumY;
    double maximumY;
    bool showAxes;
    bool showGrid;
    bool showPointMarkers;
    bool showYScale;
    double yTickInterval;
    double yLabelInterval;
    unsigned int yLabelDecimalPlaces;
    uint16_t backgroundColor;
    uint16_t lineColor;
    uint16_t axisColor;
    uint16_t gridColor;
    uint16_t labelColor;
    uint16_t zeroBarColor;

    LineChartWidgetOptions();
};

// One line on a chart; series share the options' axes, ranges, and scale.
struct LineChartSeries
{
    const LineChartPoint *points;
    size_t pointCount;
    uint16_t color;
};

// Points must have finite, nondecreasing X values. Y may be finite or NaN.
// Empty data draws an empty chart, not a zero-valued line. No data is retained.
// Each call redraws only the widget rectangle; out-of-range lines are clipped.
// Draws one series in options.lineColor.
bool drawLineChartWidget(const LineChartWidgetOptions &options,
                         const LineChartPoint *points, size_t pointCount);
// Series are drawn in order, so later series appear on top where lines cross.
// Every series is validated before anything is drawn.
bool drawLineChartSeriesWidget(const LineChartWidgetOptions &options,
                         const LineChartSeries *series, size_t seriesCount);

// Shares the line chart's axes/scale. X is each bar's center; width is in X units.
// Y must be nonnegative (or NaN to omit a bar), and minimumY must be zero.
// Optional zero markers use zeroBarColor and a pixel height clipped to the plot.
bool drawBarChartWidget(const LineChartWidgetOptions &options,
                        const LineChartPoint *points, size_t pointCount,
                        double barWidth, int zeroBarHeight = 0);

#endif
