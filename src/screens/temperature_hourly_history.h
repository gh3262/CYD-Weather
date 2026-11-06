#ifndef TEMPERATURE_HOURLY_HISTORY_H
#define TEMPERATURE_HOURLY_HISTORY_H

#include "screens/weather_snapshot_history.h"

inline bool parseTemperatureHour(const char *text, int64_t &timestamp)
{
    return parseSnapshotLocalHour(text, timestamp);
}

inline const char *parseTemperatureHourlyHistory(const char *json,
                                                 SnapshotHistoryResult &result)
{
    const SnapshotHistoryField fields[] = {
        {"ftemp", HistoryAggregation::Average},
    };
    return parseWeatherSnapshotHistory(json, fields, 1, false, &result);
}

#endif
