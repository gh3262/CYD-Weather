#include "numeric_history.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <limits>

static const double PI = 3.14159265358979323846;
static const size_t MAX_BUCKETS = 512;
static const size_t MAX_BOUNDARY_IDS = 1000;

double NumericHistoryBucket::value(HistoryAggregation aggregation) const
{
    if (count == 0)
        return std::numeric_limits<double>::quiet_NaN();
    switch (aggregation)
    {
    case HistoryAggregation::Average: return sum / count;
    case HistoryAggregation::Minimum: return minimum;
    case HistoryAggregation::Maximum: return maximum;
    case HistoryAggregation::Latest: return latest;
    case HistoryAggregation::CircularAverage:
        if (std::hypot(sineSum, cosineSum) < count * 1e-9)
            return std::numeric_limits<double>::quiet_NaN();
        {
            double bearing = std::atan2(sineSum, cosineSum) * 180.0 / PI;
            bearing = std::fmod(bearing + 360.0, 360.0);
            return bearing;
        }
    }
    return std::numeric_limits<double>::quiet_NaN();
}

bool validNumericHistoryRequest(const NumericHistoryRequest &request)
{
    const bool validMode =
        request.aggregation == HistoryAggregation::Average ||
        request.aggregation == HistoryAggregation::Minimum ||
        request.aggregation == HistoryAggregation::Maximum ||
        request.aggregation == HistoryAggregation::Latest ||
        request.aggregation == HistoryAggregation::CircularAverage;
    return validMode && request.windowSeconds > 0 &&
        request.windowSeconds <= 31U * 86400U && request.bucketSeconds > 0 &&
        request.windowSeconds / request.bucketSeconds +
            (request.windowSeconds % request.bucketSeconds != 0 ? 1U : 0U) + 1U
            <= MAX_BUCKETS;
}

bool parseHistoryTimestamp(const char *text, int64_t &milliseconds)
{
    if (!text || std::strlen(text) < 20)
        return false;
    for (size_t i = 0; i < 19; ++i)
    {
        const char expected = i == 4 || i == 7 ? '-' :
            i == 10 ? 'T' : i == 13 || i == 16 ? ':' : '\0';
        if (expected ? text[i] != expected :
            !std::isdigit(static_cast<unsigned char>(text[i])))
            return false;
    }
    int year, month, day, hour, minute, second;
    if (std::sscanf(text, "%4d-%2d-%2dT%2d:%2d:%2d",
                    &year, &month, &day, &hour, &minute, &second) != 6)
        return false;
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    const int daysPerMonth[] = {31, leap ? 29 : 28, 31, 30, 31, 30,
                                31, 31, 30, 31, 30, 31};
    if (year < 1970 || month < 1 || month > 12 || day < 1 ||
        day > daysPerMonth[month - 1] || hour > 23 || minute > 59 || second > 59)
        return false;
    size_t position = 19;
    int fraction = 0;
    if (text[position] == '.')
    {
        ++position;
        size_t digits = 0;
        while (std::isdigit(static_cast<unsigned char>(text[position])))
        {
            if (digits < 3)
                fraction = fraction * 10 + text[position] - '0';
            ++digits;
            ++position;
        }
        if (digits == 0)
            return false;
        while (digits++ < 3)
            fraction *= 10;
    }
    if (text[position] != 'Z' || text[position + 1] != '\0')
        return false;
    year -= month <= 2;
    const int era = year / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const int64_t days = era * 146097LL + doe - 719468;
    milliseconds = ((days * 24 + hour) * 3600 + minute * 60 + second) * 1000 +
        fraction;
    return true;
}

bool historyNextCursor(const char *link, std::string &cursor)
{
    cursor.clear();
    if (!link || !*link)
        return true;
    const std::string header(link);
    const size_t relation = header.find("rel=\"next\"");
    if (relation == std::string::npos)
        return true;
    // Adafruit documents rel="next" before the following comma-separated URL.
    const size_t comma = header.find(',', relation);
    size_t open = header.rfind('<', relation);
    if (comma != std::string::npos)
    {
        const size_t followingEnd = header.find(',', comma + 1);
        const std::string following = header.substr(
            comma + 1, followingEnd - comma - 1);
        if (following.find("rel=") == std::string::npos &&
            following.find("before=") != std::string::npos)
            open = header.find('<', comma);
    }
    const size_t close = open == std::string::npos ?
        std::string::npos : header.find('>', open);
    if (close == std::string::npos)
        return false;
    const std::string url = header.substr(open + 1, close - open - 1);
    size_t parameter = url.find("?before=");
    if (parameter == std::string::npos)
        parameter = url.find("&before=");
    if (parameter == std::string::npos)
        return false;
    parameter += 8;
    const size_t end = url.find('&', parameter);
    cursor = url.substr(parameter, end - parameter);
    if (cursor.empty() || cursor.size() > 512)
        return false;
    for (char c : cursor)
    {
        if (!std::isalnum(static_cast<unsigned char>(c)) &&
            c != '%' && c != '-' && c != '_' && c != '.' && c != '~' &&
            c != '+' && c != '/' && c != '=')
            return false;
    }
    return true;
}

bool NumericHistoryAccumulator::begin(const NumericHistoryRequest &request,
                                       int64_t endUtc)
{
    if (!validNumericHistoryRequest(request) ||
        endUtc <= request.windowSeconds || endUtc > 253402300799LL)
        return false;
    _result = {};
    _result.startUtc = endUtc - request.windowSeconds;
    _result.endUtc = endUtc;
    _result.bucketSeconds = request.bucketSeconds;
    _result.aggregation = request.aggregation;
    const int64_t first = _result.startUtc / request.bucketSeconds *
        request.bucketSeconds;
    const size_t count = static_cast<size_t>(
        (endUtc - 1 - first) / request.bucketSeconds + 1);
    _result.buckets.resize(count);
    for (size_t i = 0; i < count; ++i)
        _result.buckets[i].startUtc = first + i * request.bucketSeconds;
    _oldestMs = INT64_MAX;
    _boundaryIds.clear();
    return true;
}

bool NumericHistoryAccumulator::add(const char *id, const char *createdAt,
                                     const char *text)
{
    int64_t timestamp;
    if (_result.buckets.empty() || !id || !*id || std::strlen(id) > 64 ||
        !text || !parseHistoryTimestamp(createdAt, timestamp))
        return false;
    // Requests and pages are descending. Any overlap newer than the frontier
    // was already processed; remember IDs only at the frontier timestamp.
    if (timestamp > _oldestMs)
        return true;
    if (timestamp < _oldestMs)
    {
        _oldestMs = timestamp;
        _boundaryIds.clear();
    }
    if (std::find(_boundaryIds.begin(), _boundaryIds.end(), id) != _boundaryIds.end())
        return true;
    if (_boundaryIds.size() >= MAX_BOUNDARY_IDS)
        return false;
    _boundaryIds.emplace_back(id);
    if (timestamp < _result.startUtc * 1000 || timestamp >= _result.endUtc * 1000)
        return true;
    char *end = nullptr;
    const double value = std::strtod(text, &end);
    const bool parsed = end != text;
    while (end && std::isspace(static_cast<unsigned char>(*end)))
        ++end;
    if (!parsed || !end || *end != '\0' || !std::isfinite(value) ||
        (_result.aggregation == HistoryAggregation::CircularAverage &&
         (value < 0 || value > 360)))
    {
        ++_result.invalidValueCount;
        return true;
    }
    const size_t index = static_cast<size_t>(
        (timestamp / 1000 - _result.buckets.front().startUtc) /
        _result.bucketSeconds);
    NumericHistoryBucket &bucket = _result.buckets[index];
    const double sum = bucket.sum + value;
    if (!std::isfinite(sum))
        return false;
    if (bucket.count == 0)
    {
        bucket.minimum = bucket.maximum = bucket.latest = value;
        bucket.latestAtMs = timestamp;
    }
    bucket.sum = sum;
    bucket.minimum = std::fmin(bucket.minimum, value);
    bucket.maximum = std::fmax(bucket.maximum, value);
    if (timestamp > bucket.latestAtMs)
    {
        bucket.latest = value;
        bucket.latestAtMs = timestamp;
    }
    if (_result.aggregation == HistoryAggregation::CircularAverage)
    {
        bucket.sineSum += std::sin(value * PI / 180.0);
        bucket.cosineSum += std::cos(value * PI / 180.0);
    }
    ++bucket.count;
    if (_result.sampleCount == 0)
        _result.minimum = _result.maximum = value;
    _result.minimum = std::fmin(_result.minimum, value);
    _result.maximum = std::fmax(_result.maximum, value);
    ++_result.sampleCount;
    return true;
}
