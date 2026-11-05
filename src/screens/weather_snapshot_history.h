#ifndef WEATHER_SNAPSHOT_HISTORY_H
#define WEATHER_SNAPSHOT_HISTORY_H

#include <ArduinoJson.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
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
    bool nullable;
};

inline const char *parseWeatherSnapshotHistory(
    const char *json, const SnapshotHistoryField fields[], size_t seriesCount,
    bool daily, NumericHistoryResult results[])
{
    if (!fields || !results || !seriesCount || seriesCount > 2)
        return "invalid snapshot series";
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, json);
    if (error)
        return error.c_str();
    if (!document.is<JsonArray>())
        return "expected a history array";

    struct Reading
    {
        int64_t timestamp;
        double values[2];
    };
    std::vector<Reading> readings;
    readings.reserve(document.size());
    for (JsonVariantConst entry : document.as<JsonArrayConst>())
    {
        Reading reading = {};
        if (!entry.is<JsonObjectConst>() || !entry["ts"].is<const char *>() ||
            !(daily ? parseSnapshotDate(entry["ts"].as<const char *>(), reading.timestamp)
                    : parseSnapshotLocalHour(entry["ts"].as<const char *>(), reading.timestamp)))
            return daily ? "invalid local YYYYMMDD date" : "invalid local YYYYMMDDHH hour";
        for (size_t s = 0; s < seriesCount; ++s)
        {
            const JsonVariantConst value = entry[fields[s].key];
            if (fields[s].nullable && !value.isUnbound() &&
                value.isNull())
                reading.values[s] = NAN;
            else if (!value.is<double>() || !std::isfinite(value.as<double>()))
                return "expected a finite numeric history value";
            else
                reading.values[s] = value.as<double>();
        }
        readings.push_back(reading);
    }
    std::sort(readings.begin(), readings.end(),
              [](const Reading &a, const Reading &b) { return a.timestamp < b.timestamp; });
    for (size_t i = 1; i < readings.size(); ++i)
        if (readings[i - 1].timestamp == readings[i].timestamp)
            return "duplicate snapshot timestamp";

    const uint32_t interval = daily ? 86400U : 3600U;
    const size_t buckets = daily ? 30 : 72;
    int64_t end = readings.empty() ? 0 : readings.back().timestamp + interval;
    if (daily && readings.empty() &&
        !completedHistoryEndUtc(static_cast<int64_t>(std::time(nullptr)), end))
        return "local clock unavailable for empty daily snapshot";
    NumericHistoryResult parsed[2];
    for (size_t s = 0; s < seriesCount; ++s)
    {
        NumericHistoryResult &result = parsed[s];
        result.aggregation = fields[s].aggregation;
        result.bucketSeconds = interval;
        if (!end)
            continue;
        result.endUtc = end;
        result.startUtc = end - buckets * interval;
        result.buckets.resize(buckets);
        for (size_t i = 0; i < buckets; ++i)
            result.buckets[i].startUtc = result.startUtc + i * interval;
        for (const Reading &reading : readings)
        {
            if (reading.timestamp < result.startUtc || std::isnan(reading.values[s]))
                continue;
            NumericHistoryBucket &bucket =
                result.buckets[(reading.timestamp - result.startUtc) / interval];
            bucket.count = 1;
            bucket.sum = bucket.minimum = bucket.maximum = bucket.latest = reading.values[s];
            bucket.latestAtMs = reading.timestamp * 1000;
            if (result.sampleCount++ == 0)
                result.minimum = result.maximum = reading.values[s];
            else
            {
                result.minimum = std::min(result.minimum, reading.values[s]);
                result.maximum = std::max(result.maximum, reading.values[s]);
            }
        }
    }
    for (size_t s = 0; s < seriesCount; ++s)
        results[s] = std::move(parsed[s]);
    return nullptr;
}

#endif
