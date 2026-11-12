#ifndef CLOUDS_HISTORY_H
#define CLOUDS_HISTORY_H

#include "screens/daily_forecast_history.h"

inline const char *parseCloudsHistory(
    const char *json, SnapshotHistoryResult results[],
    ArduinoJson::Allocator *allocator = nullptr)
{
    const SnapshotHistoryField fields[] = {
        {"cc_min", HistoryAggregation::Minimum},
        {"cc_max", HistoryAggregation::Maximum},
        {"pct_ss", HistoryAggregation::Average},
    };
    return parseDailyForecastHistory(json, fields, 3, results, true, allocator);
}

#endif
