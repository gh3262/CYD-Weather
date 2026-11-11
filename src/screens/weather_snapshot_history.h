#ifndef WEATHER_SNAPSHOT_HISTORY_H
#define WEATHER_SNAPSHOT_HISTORY_H

#include <ArduinoJson.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <new>
#include <utility>

#include "adafruitio/numeric_history.h"

// Local calendar hours cannot distinguish repeated DST hours.
inline bool parseSnapshotLocalHour(const char *text, int64_t &timestamp)
{
    if (!text || std::strlen(text) != 10)
        return false;
    for (size_t i = 0; i < 10; ++i)
        if (text[i] < '0' || text[i] > '9')
            return false;
    char iso[24];
    std::snprintf(iso, sizeof(iso), "%.4s-%.2s-%.2sT%.2s:00:00Z",
                  text, text + 4, text + 6, text + 8);
    int64_t validated;
    if (!parseHistoryTimestamp(iso, validated))
        return false;
    struct tm hour = {};
    int year, month, day, h;
    if (std::sscanf(text, "%4d%2d%2d%2d", &year, &month, &day, &h) != 4)
        return false;
    hour.tm_year = year - 1900;
    hour.tm_mon = month - 1;
    hour.tm_mday = day;
    hour.tm_hour = h;
    // Prefer standard time for an ambiguous fall-back hour.
    for (int dst : {0, -1})
    {
        struct tm local = hour;
        local.tm_isdst = dst;
        const time_t converted = std::mktime(&local);
        char roundTrip[16];
        if (converted != static_cast<time_t>(-1) &&
            std::strftime(roundTrip, sizeof(roundTrip), "%Y%m%d%H", &local) &&
            std::strcmp(roundTrip, text) == 0)
        {
            timestamp = static_cast<int64_t>(converted);
            return true;
        }
    }
    return false;
}

// Encode a local date as UTC midnight, matching the chart's date-only labels.
inline bool parseSnapshotDate(const char *text, int64_t &timestamp)
{
    if (!text || std::strlen(text) != 8)
        return false;
    for (size_t i = 0; i < 8; ++i)
        if (text[i] < '0' || text[i] > '9')
            return false;
    char iso[24];
    std::snprintf(iso, sizeof(iso), "%.4s-%.2s-%.2sT00:00:00Z",
                  text, text + 4, text + 6);
    int64_t milliseconds;
    if (!parseHistoryTimestamp(iso, milliseconds))
        return false;
    timestamp = milliseconds / 1000;
    return true;
}

struct SnapshotHistoryField
{
    const char *key;
    HistoryAggregation aggregation;
};

static const size_t SNAPSHOT_HISTORY_BUCKETS = 72;

struct SnapshotHistoryBucket
{
    int64_t startUtc = 0;
    double latest = 0;
    uint32_t count = 0;

    double value(HistoryAggregation) const { return count ? latest : NAN; }
};

class SnapshotHistoryBuckets
{
public:
    size_t size() const { return _size; }
    bool empty() const { return _size == 0; }
    void resize(size_t size) { _size = size; }
    SnapshotHistoryBucket *begin() { return _storage.data(); }
    SnapshotHistoryBucket *end() { return begin() + _size; }
    const SnapshotHistoryBucket *begin() const { return _storage.data(); }
    const SnapshotHistoryBucket *end() const { return begin() + _size; }
    SnapshotHistoryBucket &operator[](size_t i) { return _storage[i]; }
    const SnapshotHistoryBucket &operator[](size_t i) const { return _storage[i]; }
    const SnapshotHistoryBucket &front() const { return _storage[0]; }
    const SnapshotHistoryBucket &back() const { return _storage[_size - 1]; }

private:
    std::array<SnapshotHistoryBucket, SNAPSHOT_HISTORY_BUCKETS> _storage = {};
    size_t _size = 0;
};

// Same chart metadata as NumericHistoryResult, without throwing vector growth.
struct SnapshotHistoryResult
{
    int64_t startUtc = 0;
    int64_t endUtc = 0;
    uint32_t bucketSeconds = 3600;
    HistoryAggregation aggregation = HistoryAggregation::Average;
    SnapshotHistoryBuckets buckets;
    uint32_t sampleCount = 0;
    uint32_t invalidValueCount = 0;
    double minimum = 0;
    double maximum = 0;
};

class SnapshotJsonReader
{
public:
    explicit SnapshotJsonReader(const char *text) : cursor(text ? text : "") {}
    int read() { return *cursor ? static_cast<unsigned char>(*cursor++) : -1; }
    size_t readBytes(char *buffer, size_t count)
    {
        size_t copied = 0;
        while (copied < count && *cursor)
            buffer[copied++] = *cursor++;
        return copied;
    }
    void skipWhitespace()
    {
        while (std::isspace(static_cast<unsigned char>(*cursor)))
            ++cursor;
    }
    const char *cursor;
};

template <typename Visitor>
inline const char *visitSnapshotReadings(
    const char *json, const SnapshotHistoryField fields[], size_t seriesCount,
    bool daily, Visitor visit, ArduinoJson::Allocator *allocator)
{
    SnapshotJsonReader reader(json);
    reader.skipWhitespace();
    if (reader.read() != '[')
        return "expected a history array";
    reader.skipWhitespace();
    bool first = true;
    // Deserialize only one object at a time; the producer may send weeks of hours.
    while (*reader.cursor != ']')
    {
        if (!first)
        {
            if (reader.read() != ',')
                return "expected comma between history records";
            reader.skipWhitespace();
        }
        if (*reader.cursor != '{')
            return "expected a history object";
        JsonDocument document = allocator ? JsonDocument(allocator) : JsonDocument();
        const DeserializationError error = deserializeJson(document, reader);
        if (error)
            return error.c_str();
        int64_t timestamp;
        if (!document["ts"].is<const char *>() ||
            !(daily ? parseSnapshotDate(document["ts"].as<const char *>(), timestamp)
                    : parseSnapshotLocalHour(document["ts"].as<const char *>(), timestamp)))
            return daily ? "invalid local YYYYMMDD date" : "invalid local YYYYMMDDHH hour";
        double values[3] = {};
        for (size_t s = 0; s < seriesCount; ++s)
        {
            const JsonVariantConst value = document[fields[s].key];
            if (!value.isUnbound() && value.isNull())
                values[s] = NAN;
            else if (!value.is<double>() || !std::isfinite(value.as<double>()))
                return "expected a finite numeric history value";
            else
                values[s] = value.as<double>();
        }
        visit(timestamp, values);
        first = false;
        reader.skipWhitespace();
    }
    ++reader.cursor;
    reader.skipWhitespace();
    return *reader.cursor ? "unexpected data after history array" : nullptr;
}

inline const char *parseWeatherSnapshotHistory(
    const char *json, const SnapshotHistoryField fields[], size_t seriesCount,
    bool daily, SnapshotHistoryResult results[], ArduinoJson::Allocator *allocator = nullptr)
{
    if (!fields || !results || !seriesCount || seriesCount > 2)
        return "invalid snapshot series";
    const uint32_t interval = daily ? 86400U : 3600U;
    const size_t limit = daily ? 30 : 72;
    std::unique_ptr<SnapshotHistoryResult[]> parsed(
        new (std::nothrow) SnapshotHistoryResult[seriesCount]);
    if (!parsed)
        return "insufficient memory for snapshot staging";
    size_t count = 0;
    int64_t newest = 0;
    // The producer supplies ordered, unique, already-filled readings.
    const char *error = visitSnapshotReadings(json, fields, seriesCount, daily,
        [&](int64_t timestamp, const double values[]) {
            newest = timestamp;
            for (size_t s = 0; s < seriesCount; ++s)
            {
                SnapshotHistoryBucket &bucket = parsed[s].buckets[count % limit];
                bucket.startUtc = timestamp;
                bucket.count = std::isnan(values[s]) ? 0 : 1;
                bucket.latest = values[s];
            }
            ++count;
        }, allocator);
    if (error)
        return error;
    int64_t end = count ? newest + interval : 0;
    if (daily && !count &&
        !completedHistoryEndUtc(static_cast<int64_t>(std::time(nullptr)), end))
        return "local clock unavailable for empty daily snapshot";
    for (size_t s = 0; s < seriesCount; ++s)
    {
        SnapshotHistoryResult &result = parsed[s];
        result.aggregation = fields[s].aggregation;
        result.bucketSeconds = interval;
        result.endUtc = end;
        result.startUtc = end ? end - limit * interval : 0;
        result.buckets.resize(std::min(count, limit));
        if (count > limit)
            std::rotate(result.buckets.begin(), result.buckets.begin() + count % limit,
                        result.buckets.end());
        const auto first = std::find_if(result.buckets.begin(), result.buckets.end(),
            [&](const SnapshotHistoryBucket &bucket) { return bucket.startUtc >= result.startUtc; });
        const size_t visible = result.buckets.end() - first;
        if (first != result.buckets.begin())
            std::move(first, result.buckets.end(), result.buckets.begin());
        result.buckets.resize(visible);
        for (const SnapshotHistoryBucket &bucket : result.buckets)
        {
            if (!bucket.count)
                continue;
            if (result.sampleCount++ == 0)
                result.minimum = result.maximum = bucket.latest;
            else
            {
                result.minimum = std::min(result.minimum, bucket.latest);
                result.maximum = std::max(result.maximum, bucket.latest);
            }
        }
    }
    for (size_t s = 0; s < seriesCount; ++s)
        results[s] = std::move(parsed[s]);
    return nullptr;
}

#endif
