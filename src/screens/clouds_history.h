#ifndef CLOUDS_HISTORY_H
#define CLOUDS_HISTORY_H

#include "screens/weather_snapshot_history.h"

inline const char *parseCloudsHistory(
    const char *json, SnapshotHistoryResult results[],
    ArduinoJson::Allocator *allocator = nullptr)
{
    if (!results)
        return "missing clouds results";
    std::unique_ptr<SnapshotHistoryResult[]> parsed(
        new (std::nothrow) SnapshotHistoryResult[3]);
    if (!parsed)
        return "insufficient memory for clouds staging";
    const SnapshotHistoryField fields[] = {
        {"cc_min", HistoryAggregation::Minimum},
        {"cc_max", HistoryAggregation::Maximum},
        {"pct_ss", HistoryAggregation::Average},
    };
    size_t count = 0;
    int64_t last = 0;
    const char *validationError = nullptr;
    const char *error = visitSnapshotReadings(json, fields, 3, true,
        [&](int64_t timestamp, const double values[]) {
            if (validationError)
                return;
            if (count == SNAPSHOT_HISTORY_BUCKETS)
                validationError = "too many clouds dates";
            else if (count && timestamp <= last)
                validationError = "clouds dates must be ordered and unique";
            else if (values[0] > values[1])
                validationError = "cloud cover minimum exceeds maximum";
            for (size_t s = 0; s < 3; ++s)
                if (!std::isnan(values[s]) && (values[s] < 0 || values[s] > 100))
                    validationError = "clouds percentages must be between 0 and 100";
            if (validationError)
                return;
            for (size_t s = 0; s < 3; ++s)
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
    for (size_t s = 0; s < 3; ++s)
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
