#ifndef TEMPERATURE_HOURLY_HISTORY_H
#define TEMPERATURE_HOURLY_HISTORY_H

#include <ArduinoJson.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <utility>

#include "adafruitio/numeric_history.h"

// ts has local calendar hours, with no offset to distinguish repeated DST hours.
inline bool parseTemperatureHour(const char *text, int64_t &timestamp)
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

// Validate the full snapshot before publishing; missing hours remain gaps.
inline const char *parseTemperatureHourlyHistory(const char *json,
                                                 NumericHistoryResult &result)
{
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, json);
    if (error)
        return error.c_str();
    if (!document.is<JsonArray>())
        return "expected an array of hourly temperatures";

    std::vector<std::pair<int64_t, double>> readings;
    readings.reserve(document.size());
    for (JsonVariantConst entry : document.as<JsonArrayConst>())
    {
        int64_t timestamp;
        if (!entry.is<JsonObjectConst>() || !entry["ts"].is<const char *>() ||
            !parseTemperatureHour(entry["ts"].as<const char *>(), timestamp))
            return "invalid local YYYYMMDDHH timestamp";
        if (!entry["ftemp"].is<double>())
            return "expected a numeric ftemp";
        const double value = entry["ftemp"].as<double>();
        if (!std::isfinite(value))
            return "temperature is not finite";
        readings.emplace_back(timestamp, value);
    }
    std::sort(readings.begin(), readings.end());
    for (size_t i = 1; i < readings.size(); ++i)
        if (readings[i - 1].first == readings[i].first)
            return "duplicate hourly timestamp";

    NumericHistoryResult parsed;
    if (!readings.empty())
    {
        parsed.endUtc = readings.back().first + 3600;
        parsed.startUtc = parsed.endUtc - 72 * 3600;
        parsed.buckets.resize(72);
        for (size_t i = 0; i < parsed.buckets.size(); ++i)
            parsed.buckets[i].startUtc = parsed.startUtc + i * 3600;
        for (const auto &reading : readings)
        {
            if (reading.first < parsed.startUtc)
                continue;
            NumericHistoryBucket &bucket =
                parsed.buckets[(reading.first - parsed.startUtc) / 3600];
            bucket.count = 1;
            bucket.sum = bucket.minimum = bucket.maximum = bucket.latest = reading.second;
            bucket.latestAtMs = reading.first * 1000;
            if (parsed.sampleCount++ == 0)
                parsed.minimum = parsed.maximum = reading.second;
            else
            {
                parsed.minimum = std::min(parsed.minimum, reading.second);
                parsed.maximum = std::max(parsed.maximum, reading.second);
            }
        }
    }
    result = std::move(parsed);
    return nullptr;
}

#endif
