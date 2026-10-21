#ifndef WIND_DIRECTION_STATS_H
#define WIND_DIRECTION_STATS_H

#include <stddef.h>
#include <stdint.h>

static const size_t WIND_ROSE_SECTORS = 16;

struct WindDirectionSample
{
    // Meteorological bearing the wind blows from, 0-360 degrees.
    double bearing;
    double speed;
};

struct WindDirectionStats
{
    size_t windySamples = 0;
    // Samples below the calm speed; excluded from every other statistic.
    size_t calmSamples = 0;
    // Speed-weighted vector mean; false when all calm or the winds cancel.
    bool hasAverage = false;
    double averageBearing = 0;
    // Resultant length / total speed: 1 = one direction, 0 = no prevailing one.
    double steadiness = 0;
    // Smallest clockwise arc starting at rangeStart that covers every sample.
    double rangeStart = 0;
    double rangeSpan = 0;
    uint32_t sectorCounts[WIND_ROSE_SECTORS] = {};
    uint32_t maxSectorCount = 0;
};

WindDirectionStats calculateWindDirectionStats(
    const WindDirectionSample *samples, size_t count, double calmSpeed);
// 0 = N, 1 = NNE, ... 15 = NNW; each sector is 22.5 degrees centred on its point.
size_t windRoseSector(double bearing);
const char *windCompassPoint(double bearing);

#endif
