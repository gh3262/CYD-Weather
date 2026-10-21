#include "screens/wind_direction_stats.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static const double TEST_PI = 3.14159265358979323846;
static size_t checks = 0;

static void check(bool condition, const char *message)
{
    ++checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static bool near(double actual, double expected, double tolerance = 0.01)
{
    return std::fabs(actual - expected) <= tolerance;
}

static void testCompassPoints()
{
    check(std::strcmp(windCompassPoint(0), "N") == 0, "0 is N");
    check(std::strcmp(windCompassPoint(360), "N") == 0, "360 is N");
    check(std::strcmp(windCompassPoint(11.24), "N") == 0, "N sector upper edge");
    check(std::strcmp(windCompassPoint(11.25), "NNE") == 0, "NNE sector lower edge");
    check(std::strcmp(windCompassPoint(348.75), "N") == 0, "N sector lower edge");
    check(std::strcmp(windCompassPoint(292.5), "WNW") == 0, "WNW");
    check(std::strcmp(windCompassPoint(std::nan("")), "--") == 0, "NaN");
    check(windRoseSector(180) == 8, "S sector");
}

static void testWrapAroundAverageAndRange()
{
    const WindDirectionSample samples[] = {{350, 10}, {10, 10}};
    const WindDirectionStats stats = calculateWindDirectionStats(samples, 2, 1);
    check(stats.hasAverage, "average exists");
    check(near(stats.averageBearing, 0) || near(stats.averageBearing, 360),
          "350 and 10 average to N, not S");
    check(near(stats.rangeStart, 350) && near(stats.rangeSpan, 20),
          "range wraps through N");
    check(near(stats.steadiness, std::cos(10 * TEST_PI / 180)),
          "steadiness is resultant / total speed");
    check(stats.sectorCounts[0] == 2 && stats.maxSectorCount == 2, "both in N petal");
}

static void testSpeedWeighting()
{
    // A strong north wind outweighs a light east breeze.
    const WindDirectionSample samples[] = {{0, 20}, {90, 2}};
    const WindDirectionStats stats = calculateWindDirectionStats(samples, 2, 1);
    check(near(stats.averageBearing, std::atan2(2.0, 20.0) * 180 / TEST_PI),
          "speed-weighted mean leans north");
    check(near(stats.rangeStart, 0) && near(stats.rangeSpan, 90), "range N to E");
}

static void testCalmAndInvalid()
{
    const WindDirectionSample samples[] = {
        {200, 0.5}, {270, 8}, {-5, 8}, {400, 8},
        {std::numeric_limits<double>::quiet_NaN(), 8}, {90, -1},
    };
    const WindDirectionStats stats = calculateWindDirectionStats(samples, 6, 1);
    check(stats.calmSamples == 1 && stats.windySamples == 1, "calm and invalid excluded");
    check(near(stats.averageBearing, 270) && near(stats.steadiness, 1), "single sample");
    check(near(stats.rangeSpan, 0), "single sample has zero span");

    const WindDirectionSample calm[] = {{100, 0}, {300, 0.2}};
    const WindDirectionStats calmStats = calculateWindDirectionStats(calm, 2, 1);
    check(!calmStats.hasAverage && calmStats.windySamples == 0 &&
          calmStats.calmSamples == 2, "all calm has no average");
    check(calmStats.maxSectorCount == 0, "calm adds no petals");
}

static void testCancellingWinds()
{
    const WindDirectionSample samples[] = {{90, 10}, {270, 10}};
    const WindDirectionStats stats = calculateWindDirectionStats(samples, 2, 1);
    check(!stats.hasAverage, "opposing equal winds have no average");
    check(near(stats.steadiness, 0), "opposing winds are unsteady");
    check(near(stats.rangeSpan, 180), "opposing range is half the dial");
}

static void testRangeLargestGap()
{
    const WindDirectionSample samples[] = {{300, 5}, {20, 5}, {60, 5}, {330, 5}};
    const WindDirectionStats stats = calculateWindDirectionStats(samples, 4, 1);
    check(near(stats.rangeStart, 300) && near(stats.rangeSpan, 120),
          "range skips the largest empty gap");
    check(stats.sectorCounts[windRoseSector(300)] == 1 &&
          stats.sectorCounts[windRoseSector(20)] == 1, "petal counts");
}

int main()
{
    testCompassPoints();
    testWrapAroundAverageAndRange();
    testSpeedWeighting();
    testCalmAndInvalid();
    testCancellingWinds();
    testRangeLargestGap();
    std::printf("%u wind direction checks passed\n", static_cast<unsigned>(checks));
    return 0;
}
