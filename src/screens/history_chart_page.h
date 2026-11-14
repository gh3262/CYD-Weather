#ifndef HISTORY_CHART_PAGE_H
#define HISTORY_CHART_PAGE_H

#include <Arduino.h>
#include <math.h>

#include "adafruitio/adafruitio_manager.h"

struct SnapshotHistoryResult;
struct HistoryChartScale
{
    double tickInterval;
    double labelInterval;
};

struct HistoryChartSeries
{
    const char *feedKey;
    HistoryAggregation aggregation;
    uint16_t color;
    // Legend text for multi-series pages; single-series pages show Min/Max.
    const char *label;
};

enum class HistoryChartStyle
{
    Line,
    DailyBars,
    DailyFloatingBars,
    DailyForecastFloatingBars,
    DailyForecastLine,
    PressureTrendBars,
    CloudBarsLine,
    DailyPrecipitationBars
};

struct HistoryChartPage
{
    const char *title;
    // Up to two numeric-history series, or three snapshot series.
    const HistoryChartSeries *series;
    size_t seriesCount;
    uint32_t windowSeconds;
    uint32_t bucketSeconds;
    // Used in empty-window messages, such as "No readings in last 24 hours".
    const char *windowText;
    // Appended to observed values, such as " F".
    const char *unitSuffix;
    unsigned int valueDecimals;
    // Added above the observed maximum, and below the minimum unless zero-based.
    double yPadding;
    bool zeroBasedY;
    // Returns Y tick/label spacing for the padded chart range.
    HistoryChartScale (*scale)(double minimumY, double maximumY);
    HistoryChartStyle style;
};

// Only one history chart can be active; drawing a page replaces the previous one.
void drawHistoryChartPage(const HistoryChartPage &page);
// For pages supplied by a snapshot rather than a numeric-history subscription.
void drawHistoryChartFrame(const HistoryChartPage &page);
void refreshHistoryChartData(const HistoryChartPage &page,
                            const NumericHistoryResult histories[],
                            const NumericHistoryStatus &status, bool stale);
void refreshHistoryChartData(const HistoryChartPage &page,
                            const SnapshotHistoryResult histories[],
                            const NumericHistoryStatus &status, bool stale,
                            double referenceLineY = NAN);
// Redraws only when history status, data presence, or staleness changes.
void refreshHistoryChartPage(const HistoryChartPage &page);
void leaveHistoryChartPage();

#endif
