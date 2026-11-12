#ifndef DAILY_FORECAST_HISTORY_H
#define DAILY_FORECAST_HISTORY_H

#include "screens/weather_snapshot_history.h"

inline const char *parseDailyForecastHistory(
    const char *json, const SnapshotHistoryField fields[], size_t seriesCount,
    SnapshotHistoryResult results[], bool percentages = false,
    ArduinoJson::Allocator *allocator = nullptr)
{
    if (!fields || !results || seriesCount < 2 || seriesCount > 3)
        return "invalid daily forecast series";
    std::unique_ptr<SnapshotHistoryResult[]> parsed(
        new (std::nothrow) SnapshotHistoryResult[seriesCount]);
    if (!parsed)
        return "insufficient memory for daily forecast staging";
    size_t count = 0;
    int64_t last = 0;
    const char *validationError = nullptr;
    const char *error = visitSnapshotReadings(json, fields, seriesCount, true,
        [&](int64_t timestamp, const double values[]) {
            if (validationError)
                return;
            if (count == SNAPSHOT_HISTORY_BUCKETS)
                validationError = "too many daily forecast dates";
            else if (count && timestamp <= last)
                validationError = "daily forecast dates must be ordered and unique";
            else if (fields[0].aggregation == HistoryAggregation::Minimum &&
                     fields[1].aggregation == HistoryAggregation::Maximum &&
                     values[0] > values[1])
                validationError = "daily forecast minimum exceeds maximum";
            for (size_t s = 0; percentages && s < seriesCount; ++s)
                if (!std::isnan(values[s]) && (values[s] < 0 || values[s] > 100))
                    validationError = "daily forecast percentages must be between 0 and 100";
            if (validationError)
                return;
            for (size_t s = 0; s < seriesCount; ++s)
            {
                auto &result = parsed[s];
                auto &bucket = result.buckets[count];
                bucket.startUtc = timestamp;
                bucket.latest = values[s];
                bucket.count = std::isnan(values[s]) ? 0 : 1;
                if (!count)
                    result.startUtc = timestamp;
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
        }, allocator);
    if (error || validationError)
        return error ? error : validationError;
    for (size_t s = 0; s < seriesCount; ++s)
    {
        parsed[s].endUtc = count ? last + 86400 : 0;
        parsed[s].bucketSeconds = 86400;
        parsed[s].aggregation = fields[s].aggregation;
        parsed[s].buckets.resize(count);
        results[s] = std::move(parsed[s]);
    }
    return nullptr;
}

#endif
