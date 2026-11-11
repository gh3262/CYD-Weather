#ifndef BAROMETER_TREND_HISTORY_H
#define BAROMETER_TREND_HISTORY_H

#include "screens/weather_snapshot_history.h"

static const size_t PRESSURE_TREND_SERIES_COUNT = 3;

inline bool parsePressureTrendBucket(const char *text, int64_t &start, int64_t &end)
{
    if (!text || std::strlen(text) != 12 || text[4] != '-' ||
        text[7] != '-' || text[10] != '-' || text[11] < '1' || text[11] > '8')
        return false;
    char hour[16];
    std::snprintf(hour, sizeof(hour), "%.4s%.2s%.2s%02d",
                  text, text + 5, text + 8, (text[11] - '1') * 3);
    if (!parseSnapshotLocalHour(hour, start))
        return false;
    const time_t timestamp = static_cast<time_t>(start);
    struct tm local;
    if (!localtime_r(&timestamp, &local))
        return false;
    local.tm_hour += 3;
    local.tm_isdst = -1;
    const time_t next = std::mktime(&local);
    if (next == static_cast<time_t>(-1))
        return false;
    end = static_cast<int64_t>(next);
    return end > start;
}

// Validate the whole snapshot before replacing any previously rendered data.
inline const char *parseBarometerTrendHistory(
    const char *json, SnapshotHistoryResult results[],
    ArduinoJson::Allocator *allocator = nullptr)
{
    if (!results)
        return "missing pressure trend results";
    std::unique_ptr<SnapshotHistoryResult[]> parsed(
        new (std::nothrow) SnapshotHistoryResult[PRESSURE_TREND_SERIES_COUNT]);
    if (!parsed)
        return "insufficient memory for pressure trend staging";
    SnapshotJsonReader reader(json);
    reader.skipWhitespace();
    if (reader.read() != '[')
        return "expected a pressure trend array";
    reader.skipWhitespace();
    size_t count = 0;
    int64_t lastStart = 0;
    int64_t lastEnd = 0;
    const char *keys[] = {"min", "max", "average"};
    while (*reader.cursor != ']')
    {
        if (count)
        {
            if (reader.read() != ',')
                return "expected comma between pressure buckets";
            reader.skipWhitespace();
        }
        if (*reader.cursor != '{')
            return "expected a pressure bucket object";
        if (count == SNAPSHOT_HISTORY_BUCKETS)
            return "too many pressure trend buckets";
        JsonDocument document = allocator ? JsonDocument(allocator) : JsonDocument();
        const DeserializationError error = deserializeJson(document, reader);
        if (error)
            return error.c_str();
        int64_t start, end;
        if (!document["bucket"].is<const char *>() ||
            !parsePressureTrendBucket(document["bucket"].as<const char *>(), start, end))
            return "invalid local YYYY-MM-DD bucket (1-8)";
        if (count && start <= lastStart)
            return "pressure trend buckets must be ordered and unique";
        double values[PRESSURE_TREND_SERIES_COUNT];
        for (size_t s = 0; s < PRESSURE_TREND_SERIES_COUNT; ++s)
        {
            const JsonVariantConst value = document[keys[s]];
            if (!value.is<double>() || !std::isfinite(value.as<double>()))
                return "expected finite min, max and average";
            values[s] = value.as<double>();
        }
        if (values[0] > values[1] || values[2] < values[0] || values[2] > values[1])
            return "pressure average must be between min and max";
        for (size_t s = 0; s < PRESSURE_TREND_SERIES_COUNT; ++s)
        {
            auto &result = parsed[s];
            result.buckets[count].startUtc = start;
            result.buckets[count].latest = values[s];
            result.buckets[count].count = 1;
            if (!count)
            {
                result.startUtc = start;
                result.minimum = result.maximum = values[s];
            }
            result.minimum = std::min(result.minimum, values[s]);
            result.maximum = std::max(result.maximum, values[s]);
        }
        lastStart = start;
        lastEnd = end;
        ++count;
        reader.skipWhitespace();
    }
    ++reader.cursor;
    reader.skipWhitespace();
    if (*reader.cursor)
        return "unexpected data after pressure trend array";
    const HistoryAggregation aggregations[] = {
        HistoryAggregation::Minimum, HistoryAggregation::Maximum, HistoryAggregation::Average,
    };
    for (size_t s = 0; s < PRESSURE_TREND_SERIES_COUNT; ++s)
    {
        parsed[s].endUtc = lastEnd;
        parsed[s].bucketSeconds = 3U * 3600U;
        parsed[s].aggregation = aggregations[s];
        parsed[s].sampleCount = count;
        parsed[s].buckets.resize(count);
        results[s] = std::move(parsed[s]);
    }
    return nullptr;
}

#endif
