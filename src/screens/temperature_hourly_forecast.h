#ifndef TEMPERATURE_HOURLY_FORECAST_H
#define TEMPERATURE_HOURLY_FORECAST_H

#include "screens/weather_snapshot_history.h"

inline bool currentTemperatureForecastHour(int64_t now, int64_t &hour)
{
    int64_t date;
    if (!completedHistoryEndUtc(now, date))
        return false;
    const time_t timestamp = static_cast<time_t>(now);
    struct tm local;
    if (!localtime_r(&timestamp, &local))
        return false;
    hour = date + local.tm_hour * 3600;
    return true;
}

inline const char *parseTemperatureHourlyForecast(
    const char *json, SnapshotHistoryResult results[],
    ArduinoJson::Allocator *allocator = nullptr)
{
    const SnapshotHistoryField fields[] = {
        {"lower_bound", HistoryAggregation::Minimum},
        {"upper_bound", HistoryAggregation::Maximum},
        {"temperature_2m", HistoryAggregation::Average},
    };
    std::unique_ptr<SnapshotHistoryResult[]> parsed(
        new (std::nothrow) SnapshotHistoryResult[3]);
    if (!parsed)
        return "insufficient memory for hourly temperature staging";
    size_t count = 0;
    int64_t start = 0;
    int64_t last = 0;
    const char *validationError = nullptr;
    const char *error = visitSnapshotReadings(json, fields, 3, false,
        [&](int64_t timestamp, const double values[]) {
            if (validationError)
                return;
            if (!count)
                start = timestamp - timestamp % 86400;
            if (count == 24)
                validationError = "too many hourly temperatures";
            else if (count && timestamp <= last)
                validationError = "hourly temperatures must be ordered and unique";
            else if (timestamp >= start + 86400)
                validationError = "hourly temperatures must share one local date";
            else if (values[0] > values[1])
                validationError = "hourly lower bound exceeds upper bound";
            if (validationError)
                return;
            for (size_t s = 0; s < 3; ++s)
            {
                auto &result = parsed[s];
                auto &bucket = result.buckets[count];
                bucket.startUtc = timestamp;
                bucket.latest = values[s];
                bucket.count = std::isnan(values[s]) ? 0 : 1;
                if (bucket.count)
                {
                    if (!result.sampleCount++)
                        result.minimum = result.maximum = values[s];
                    result.minimum = std::min(result.minimum, values[s]);
                    result.maximum = std::max(result.maximum, values[s]);
                }
            }
            last = timestamp;
            ++count;
        }, allocator, true);
    if (error || validationError)
        return error ? error : validationError;
    for (size_t s = 0; s < 3; ++s)
    {
        parsed[s].startUtc = count ? start : 0;
        parsed[s].endUtc = count ? start + 86400 : 0;
        parsed[s].bucketSeconds = 3600;
        parsed[s].aggregation = fields[s].aggregation;
        parsed[s].buckets.resize(count);
        results[s] = std::move(parsed[s]);
    }
    return nullptr;
}

#endif
