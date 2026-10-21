#include "wind_direction_stats.h"

#include <algorithm>
#include <cmath>
#include <vector>

static const double WIND_PI = 3.14159265358979323846;
static const double SECTOR_DEGREES = 360.0 / WIND_ROSE_SECTORS;

static double normalizeBearing(double bearing)
{
    bearing = std::fmod(bearing, 360.0);
    return bearing < 0 ? bearing + 360.0 : bearing;
}

size_t windRoseSector(double bearing)
{
    if (!std::isfinite(bearing))
        return 0;
    return static_cast<size_t>(
        normalizeBearing(bearing + SECTOR_DEGREES / 2.0) / SECTOR_DEGREES) %
        WIND_ROSE_SECTORS;
}

const char *windCompassPoint(double bearing)
{
    static const char *const POINTS[WIND_ROSE_SECTORS] = {
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW",
    };
    return std::isfinite(bearing) ? POINTS[windRoseSector(bearing)] : "--";
}

WindDirectionStats calculateWindDirectionStats(
    const WindDirectionSample *samples, size_t count, double calmSpeed)
{
    WindDirectionStats stats;
    std::vector<double> bearings;
    bearings.reserve(count);
    double east = 0;
    double north = 0;
    double totalSpeed = 0;
    for (size_t i = 0; samples && i < count; ++i)
    {
        const WindDirectionSample &sample = samples[i];
        if (!std::isfinite(sample.bearing) || !std::isfinite(sample.speed) ||
            sample.bearing < 0 || sample.bearing > 360 || sample.speed < 0)
            continue;
        if (sample.speed < calmSpeed)
        {
            ++stats.calmSamples;
            continue;
        }
        const double bearing = normalizeBearing(sample.bearing);
        const double radians = bearing * WIND_PI / 180.0;
        east += sample.speed * std::sin(radians);
        north += sample.speed * std::cos(radians);
        totalSpeed += sample.speed;
        bearings.push_back(bearing);
        const uint32_t sectorCount = ++stats.sectorCounts[windRoseSector(bearing)];
        stats.maxSectorCount = std::max(stats.maxSectorCount, sectorCount);
    }
    stats.windySamples = bearings.size();
    if (bearings.empty())
        return stats;

    const double resultant = std::hypot(east, north);
    if (totalSpeed > 0)
        stats.steadiness = std::min(1.0, resultant / totalSpeed);
    if (totalSpeed > 0 && resultant > totalSpeed * 1e-9)
    {
        stats.hasAverage = true;
        stats.averageBearing =
            normalizeBearing(std::atan2(east, north) * 180.0 / WIND_PI);
    }

    // The covering arc is everything except the largest empty gap between bearings.
    std::sort(bearings.begin(), bearings.end());
    double largestGap = bearings.front() + 360.0 - bearings.back();
    double rangeStart = bearings.front();
    for (size_t i = 1; i < bearings.size(); ++i)
    {
        const double gap = bearings[i] - bearings[i - 1];
        if (gap > largestGap)
        {
            largestGap = gap;
            rangeStart = bearings[i];
        }
    }
    stats.rangeStart = rangeStart;
    stats.rangeSpan = 360.0 - largestGap;
    return stats;
}
