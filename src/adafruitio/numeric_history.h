#ifndef NUMERIC_HISTORY_H
#define NUMERIC_HISTORY_H

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>

enum class HistoryAggregation
{
    Average,
    Minimum,
    Maximum,
    Latest,
    CircularAverage
};

enum class HistoryWindow
{
    Rolling,
    CompletedUtcDays
};

struct NumericHistoryRequest
{
    uint32_t windowSeconds = 86400;
    uint32_t bucketSeconds = 3600;
    HistoryAggregation aggregation = HistoryAggregation::Average;
    HistoryWindow window = HistoryWindow::Rolling;
};

struct NumericHistoryBucket
{
    int64_t startUtc = 0;
    uint32_t count = 0;
    double sum = 0;
    double minimum = 0;
    double maximum = 0;
    double latest = 0;
    int64_t latestAtMs = 0;
    double sineSum = 0;
    double cosineSum = 0;

    double value(HistoryAggregation aggregation) const;
};

struct NumericHistoryResult
{
    int64_t startUtc = 0;
    int64_t endUtc = 0;
    uint32_t bucketSeconds = 3600;
    HistoryAggregation aggregation = HistoryAggregation::Average;
    std::vector<NumericHistoryBucket> buckets;
    uint32_t sampleCount = 0;
    uint32_t invalidValueCount = 0;
    double minimum = 0;
    double maximum = 0;
};

bool validNumericHistoryRequest(const NumericHistoryRequest &request);
// Local today's calendar date encoded as UTC midnight, for date-only feeds.
bool completedHistoryEndUtc(int64_t nowUtc, int64_t &endUtc);
// ISO 8601 UTC, including optional fractional seconds. Never uses local TZ.
bool parseHistoryTimestamp(const char *text, int64_t &milliseconds);
// Extract only the opaque before cursor; never follow an arbitrary header URL.
bool historyNextCursor(const char *link, std::string &cursor);

class NumericHistoryAccumulator
{
public:
    bool begin(const NumericHistoryRequest &request, int64_t endUtc);
    // API records are newest first. Overlapping records are counted only once.
    bool add(const char *id, const char *createdAt, const char *value);
    const NumericHistoryResult &result() const { return _result; }

private:
    NumericHistoryResult _result;
    int64_t _oldestMs = INT64_MAX;
    std::vector<std::string> _boundaryIds;
};

#endif
