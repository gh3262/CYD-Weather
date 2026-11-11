#ifndef WIND_DIRECTION_SNAPSHOT_H
#define WIND_DIRECTION_SNAPSHOT_H

#include <ArduinoJson.h>
#include <cmath>
#include "adafruitio/numeric_history.h"
#include "screens/wind_direction_stats.h"

struct WindDirectionSnapshot
{
    int64_t endUtc = 0;
    int currentBearing = -1;
    int currentSpeed = -1;
    WindDirectionStats stats;
};

inline bool windSnapshotNumber(JsonVariantConst value, double minimum,
                               double maximum, double &number)
{
    if (!value.is<double>() || value.is<bool>())
        return false;
    number = value.as<double>();
    return std::isfinite(number) && number >= minimum && number <= maximum;
}

inline const char *parseWindDirectionSnapshot(const char *json,
                                              WindDirectionSnapshot &snapshot)
{
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, json);
    if (error)
        return error.c_str();
    if (!document.is<JsonObject>())
        return "expected a wind snapshot object";
    WindDirectionSnapshot parsed;
    int64_t endMs;
    const char *endText = document["windowEndUtc"].as<const char *>();
    if (!endText || !parseHistoryTimestamp(endText, endMs) ||
        endMs < 1577836800000LL || endMs % 1000 != 0)
        return "expected a synchronized UTC window end";
    parsed.endUtc = endMs / 1000;
    if (!document["windowSeconds"].is<unsigned>() ||
        document["windowSeconds"].as<unsigned>() != 43200U ||
        !document["bucketSeconds"].is<unsigned>() ||
        document["bucketSeconds"].as<unsigned>() != 600U)
        return "expected a 12-hour window with 10-minute buckets";
    double number;
    const JsonObjectConst object = document.as<JsonObjectConst>();
    if (!object["currentBearing"].isNull())
    {
        if (!windSnapshotNumber(object["currentBearing"], 0, 360, number))
            return "invalid current bearing";
        parsed.currentBearing = static_cast<int>(std::lround(number)) % 360;
    }
    if (!object["currentSpeedMph"].isNull())
    {
        if (!windSnapshotNumber(object["currentSpeedMph"], 0, 2147483646.0, number))
            return "invalid current speed";
        parsed.currentSpeed = static_cast<int>(std::lround(number));
    }
    if (object["currentBearing"].isUnbound() ||
        object["currentSpeedMph"].isUnbound() || object["averageBearing"].isUnbound())
        return "missing current wind fields";
    WindDirectionStats &stats = parsed.stats;
    if (!object["hasAverage"].is<bool>())
        return "expected an average availability flag";
    stats.hasAverage = object["hasAverage"].as<bool>();
    if (stats.hasAverage)
    {
        if (!windSnapshotNumber(object["averageBearing"], 0, 360, stats.averageBearing))
            return "invalid average bearing";
        stats.averageBearing = std::fmod(stats.averageBearing, 360.0);
    }
    else if (!object["averageBearing"].isNull())
        return "undefined average must be null";
    if (!windSnapshotNumber(object["steadiness"], 0, 1, stats.steadiness) ||
        !windSnapshotNumber(object["rangeStart"], 0, 360, stats.rangeStart) ||
        !windSnapshotNumber(object["rangeSpan"], 0, 360, stats.rangeSpan))
        return "invalid wind steadiness or range";
    stats.rangeStart = std::fmod(stats.rangeStart, 360.0);
    if (!object["windySamples"].is<unsigned>() ||
        !object["calmSamples"].is<unsigned>())
        return "expected wind bucket counts";
    stats.windySamples = object["windySamples"].as<unsigned>();
    stats.calmSamples = object["calmSamples"].as<unsigned>();
    if (stats.windySamples > 73 || stats.calmSamples > 73 ||
        stats.windySamples + stats.calmSamples > 73)
        return "too many 10-minute wind buckets";
    const JsonArrayConst counts = object["sectorCounts"].as<JsonArrayConst>();
    if (counts.size() != WIND_ROSE_SECTORS)
        return "expected 16 wind rose counts";
    size_t total = 0;
    for (size_t i = 0; i < WIND_ROSE_SECTORS; ++i)
    {
        if (!counts[i].is<unsigned>() || counts[i].as<unsigned>() > 73)
            return "invalid wind rose count";
        stats.sectorCounts[i] = counts[i].as<unsigned>();
        total += stats.sectorCounts[i];
        if (stats.sectorCounts[i] > stats.maxSectorCount)
            stats.maxSectorCount = stats.sectorCounts[i];
    }
    if (total != stats.windySamples ||
        (stats.hasAverage && !stats.windySamples) ||
        (!stats.windySamples &&
         (stats.steadiness != 0 || stats.rangeStart != 0 || stats.rangeSpan != 0)))
        return "inconsistent wind statistics";
    snapshot = parsed;
    return nullptr;
}

#endif
