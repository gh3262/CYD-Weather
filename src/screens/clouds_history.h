#ifndef CLOUDS_HISTORY_H
#define CLOUDS_HISTORY_H

#include "screens/daily_forecast_history.h"

inline const char *parseCloudsHistory(
    const char *json, SnapshotHistoryResult results[],
    ArduinoJson::Allocator *allocator = nullptr)
{
    const SnapshotHistoryField fields[] = {
        {"pct_ss", HistoryAggregation::Average},
        {"cc_mean", HistoryAggregation::Average},
    };
    return parseDailyForecastHistory(json, fields, 2, results, true, allocator);
}

#endif
