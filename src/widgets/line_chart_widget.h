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

    LineChartWidgetOptions();
};

// Points must have finite, nondecreasing X values. Y may be finite or NaN.
// Empty data draws an empty chart, not a zero-valued line. No data is retained.
// Each call redraws only the widget rectangle; out-of-range lines are clipped.
bool drawLineChartWidget(const LineChartWidgetOptions &options,
                         const LineChartPoint *points, size_t pointCount);

#endif
