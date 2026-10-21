#ifndef HISTORY_CHART_PAGE_H
#define HISTORY_CHART_PAGE_H

#include <Arduino.h>

#include "adafruitio/adafruitio_manager.h"

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
    DailyBars
};

struct HistoryChartPage
{
    const char *title;
    // 1 to MAX_NUMERIC_HISTORY_SERIES series; later series draw on top.
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
// Redraws only when history status, data presence, or staleness changes.
void refreshHistoryChartPage(const HistoryChartPage &page);
void leaveHistoryChartPage();

#endif
