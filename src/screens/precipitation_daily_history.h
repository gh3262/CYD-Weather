#ifndef PRECIPITATION_DAILY_HISTORY_H
#define PRECIPITATION_DAILY_HISTORY_H

#include "screens/weather_snapshot_history.h"

// Series contain cumulative rain, rain plus showers, and total wet hours.
inline const char *parsePrecipitationDailyHistory(
    const char *json, SnapshotHistoryResult results[],
    ArduinoJson::Allocator *allocator = nullptr)
{
    const SnapshotHistoryField fields[] = {
        {"precip_hours", HistoryAggregation::Average},
        {"rain_sum", HistoryAggregation::Average},
        {"showers_sum", HistoryAggregation::Average},
        {"snowfall_sum", HistoryAggregation::Average},
    };
    std::unique_ptr<SnapshotHistoryResult[]> parsed(
        new (std::nothrow) SnapshotHistoryResult[3]);
    if (!parsed)
        return "insufficient memory for daily precipitation staging";
    size_t count = 0;
    int64_t last = 0;
    const char *validationError = nullptr;
    const char *error = visitSnapshotReadings(json, fields, 4, true,
        [&](int64_t timestamp, const double values[]) {
            if (validationError)
                return;
            if (count == SNAPSHOT_HISTORY_BUCKETS)
                validationError = "too many daily precipitation dates";
            else if (count && timestamp <= last)
                validationError = "daily precipitation dates must be ordered and unique";
            else if (!std::isnan(values[0]) &&
                     (values[0] < 0 || values[0] > 24))
                validationError = "precipitation hours must be between 0 and 24";
            for (size_t i = 1; i < 4; ++i)
                if (!std::isnan(values[i]) && values[i] < 0)
                    validationError = "precipitation amounts must be nonnegative";
            if (validationError)
                return;
            const double total = values[1] + values[2] + values[3];
            if (!std::isnan(total) && !std::isfinite(total))
            {
                validationError = "precipitation total is too large";
                return;
            }
            double cumulative[3] = {NAN, NAN, values[0]};
            if (!std::isnan(values[0]) && values[0] == 0)
                cumulative[0] = cumulative[1] = 0;
            else if (!std::isnan(values[0]) && std::isfinite(total) && total > 0)
            {
                cumulative[0] = values[0] * (values[1] / total);
                cumulative[1] = values[0] * ((values[1] + values[2]) / total);
            }
            for (size_t s = 0; s < 3; ++s)
            {
                auto &result = parsed[s];
                auto &bucket = result.buckets[count];
                bucket.startUtc = timestamp;
                bucket.latest = cumulative[s];
                bucket.count = std::isnan(cumulative[s]) ? 0 : 1;
                if (!count)
                    result.startUtc = timestamp;
                if (bucket.count)
                {
                    if (!result.sampleCount++)
                        result.minimum = result.maximum = cumulative[s];
                    result.minimum = std::min(result.minimum, cumulative[s]);
                    result.maximum = std::max(result.maximum, cumulative[s]);
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
        parsed[s].aggregation = HistoryAggregation::Average;
        parsed[s].buckets.resize(count);
        results[s] = std::move(parsed[s]);
    }
    return nullptr;
}

#endif
