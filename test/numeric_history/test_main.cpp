#include "numeric_history.h"
#include "numeric_history_page.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

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

static int64_t seconds(const char *text)
{
    int64_t timestamp = 0;
    check(parseHistoryTimestamp(text, timestamp), "fixture timestamp");
    return timestamp / 1000;
}

static std::string timestamp(int64_t utc)
{
    const time_t value = static_cast<time_t>(utc);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&value));
    return buffer;
}

static std::string record(int id, int64_t utc, const char *value)
{
    return "{\"id\":\"" + std::to_string(id) + "\",\"created_at\":\"" +
        timestamp(utc) + "\",\"value\":\"" + value +
        "\",\"location\":{\"lat\":null},\"updated_at\":\"ignored\"}";
}

int main()
{
    int64_t parsed;
    check(parseHistoryTimestamp("1970-01-01T00:00:00Z", parsed) && parsed == 0,
          "Unix epoch");
    check(parseHistoryTimestamp("2024-02-29T12:30:45.123456Z", parsed) &&
          parsed % 1000 == 123, "fractional UTC timestamp");
    check(parseHistoryTimestamp("2024-02-29T12:30:45.1Z", parsed) &&
          parsed % 1000 == 100, "short fraction");
    for (const char *bad : {"2023-02-29T00:00:00Z", "2024-04-31T00:00:00Z",
                            "2024-01-01T24:00:00Z", "2024-01-01T00:00:00",
                            "2024-01-01T00:00:00.Z", "2024-01-01T00:00:00+00:00",
                            "2024-01-01T00:00:00Zjunk", "2024-1-01T00:00:00Z"})
        check(!parseHistoryTimestamp(bad, parsed), "malformed timestamp rejected");
    check(!parseHistoryTimestamp(nullptr, parsed), "null timestamp rejected");

    NumericHistoryRequest request;
    check(validNumericHistoryRequest(request), "default hourly 24h request");
    request.bucketSeconds = 60;
    check(!validNumericHistoryRequest(request), "excessive bucket count rejected");
    request.windowSeconds = 3600;
    check(validNumericHistoryRequest(request), "minute buckets supported");
    request.bucketSeconds = 0;
    check(!validNumericHistoryRequest(request), "zero interval rejected");
    request = {};
    request.windowSeconds = 32 * 86400;
    check(!validNumericHistoryRequest(request), "oversized window rejected");
    request = {};

    const int64_t end = seconds("2026-10-19T18:30:00Z");
    NumericHistoryAccumulator accumulator;
    check(accumulator.begin(request, end), "initialize window");
    check(accumulator.result().buckets.size() == 25, "partial boundary hours included");
    check(accumulator.result().buckets.front().startUtc ==
          seconds("2026-10-18T18:00:00Z"), "UTC hour alignment");
    check(accumulator.add("a", "2026-10-19T18:29:00Z", "80"), "first reading");
    check(accumulator.add("b", "2026-10-19T18:10:00Z", "60"), "partial-hour reading");
    check(accumulator.add("b", "2026-10-19T18:10:00Z", "60"), "boundary duplicate");
    check(accumulator.add("c", "2026-10-19T18:10:00Z", "60"), "separate equal reading");
    check(accumulator.add("a", "2026-10-19T18:29:00Z", "80"), "newer page overlap");
    check(accumulator.result().sampleCount == 3, "count IDs, not distinct values");
    const NumericHistoryBucket &last = accumulator.result().buckets.back();
    check(std::abs(last.value(HistoryAggregation::Average) - 200.0 / 3) < 1e-9,
          "simple partial-hour average");
    check(last.value(HistoryAggregation::Maximum) == 80, "gust maximum");
    check(last.value(HistoryAggregation::Minimum) == 60, "minimum mode");
    check(last.value(HistoryAggregation::Latest) == 80, "latest by timestamp");
    check(std::isnan(accumulator.result().buckets[20].value(
                        HistoryAggregation::Average)), "empty hours are gaps");
    check(accumulator.add("d", "2026-10-19T15:30:00Z", " 55 "), "value whitespace");
    check(accumulator.add("e", "2026-10-19T15:20:00Z", " "), "invalid value skipped");
    check(accumulator.add("f", "2026-10-19T15:10:00Z", "NaN"), "NaN skipped");
    check(accumulator.add("g", "2026-10-19T15:00:00Z", "42F"), "nonnumeric skipped");
    check(accumulator.result().invalidValueCount == 3, "invalid count retained");
    check(accumulator.result().minimum == 55 && accumulator.result().maximum == 80,
          "raw extrema across buckets");
    check(accumulator.add("h", "2026-10-18T18:30:00Z", "10"), "include exact start");
    check(accumulator.add("i", "2026-10-18T18:29:59Z", "-200"), "exclude before window");
    check(accumulator.result().minimum == 10, "outside values do not change extrema");

    request.aggregation = HistoryAggregation::CircularAverage;
    check(accumulator.begin(request, end), "circular mode");
    check(accumulator.add("a", "2026-10-19T18:20:00Z", "350"), "350-degree bearing");
    check(accumulator.add("b", "2026-10-19T18:10:00Z", "10"), "10-degree bearing");
    double bearing = accumulator.result().buckets.back().value(request.aggregation);
    check(bearing < 1e-8 || bearing > 360 - 1e-8, "circular average points north");
    check(accumulator.begin(request, end), "restart circular mode");
    check(accumulator.add("a", "2026-10-19T18:20:00Z", "90"), "east bearing");
    check(accumulator.add("b", "2026-10-19T18:10:00Z", "270"), "west bearing");
    check(std::isnan(accumulator.result().buckets.back().value(request.aggregation)),
          "opposing bearings undefined");
    check(accumulator.add("c", "2026-10-19T18:00:00Z", "361"), "bad bearing skipped");
    check(accumulator.result().invalidValueCount == 1, "bad bearing reported");

    request = {};
    request.bucketSeconds = 7200;
    check(accumulator.begin(request, end), "custom two-hour buckets");
    check(accumulator.result().buckets.size() == 13, "two-hour partial endpoints");
    check(accumulator.begin(request, seconds("2026-10-19T18:00:00Z")),
          "aligned window");
    check(accumulator.result().buckets.size() == 12, "no extra bucket at exact end");

    std::string cursor;
    check(historyNextCursor("", cursor) && cursor.empty(), "no next page");
    check(historyNextCursor(
        "<https://io.adafruit.com/data?limit=100>;rel=\"next\", "
        "<https://io.adafruit.com/data?before=abc%3D%3D&limit=100>", cursor) &&
        cursor == "abc%3D%3D", "documented Adafruit next-link syntax");
    check(historyNextCursor(
        "<https://io.adafruit.com/data?before=old>;rel=\"next\", "
        "<https://io.adafruit.com/data?before=new>", cursor) &&
        cursor == "new", "subsequent Adafruit cursor");
    check(historyNextCursor(
        "<https://io.adafruit.com/data?before=abc>; rel=\"next\"", cursor) &&
        cursor == "abc", "standard next-link syntax");
    check(historyNextCursor(
        "<https://io.adafruit.com/data?before=abc>; rel=\"next\", "
        "<https://io.adafruit.com/data?before=old>; rel=\"prev\"", cursor) &&
        cursor == "abc", "standard next/previous links");
    check(!historyNextCursor("<https://example.test/data>;rel=\"next\"", cursor),
          "next link missing cursor rejected");
    check(!historyNextCursor("<https://example.test/data?before=bad value>;rel=\"next\"",
                            cursor), "unsafe cursor rejected");

    request = {};
    check(accumulator.begin(request, end), "paged fixture");
    size_t total = 0;
    for (int page = 0; page < 15; ++page)
    {
        std::string json = "[";
        for (int item = 0; item < 100; ++item)
        {
            const int id = page * 100 + item;
            if (item)
                json += ",";
            json += record(id, end - 1 - id * 30, "67.5");
        }
        json += "]";
        const std::string link = page < 14 ?
            "<https://io.adafruit.com/data>;rel=\"next\", "
            "<https://io.adafruit.com/data?before=" + std::to_string(page + 1) + ">" :
            "<https://io.adafruit.com/data?before=14>";
        check(readNumericHistoryPage(json, link.c_str(), accumulator, cursor),
              "decode paginated JSON page");
        total += 100;
        check(accumulator.result().sampleCount == total, "all page samples accumulated");
    }
    check(total == 1500 && cursor.empty(), "window beyond 1000 samples complete");
    check(accumulator.result().buckets.size() == 25, "memory scales with buckets");
    check(accumulator.result().minimum == 67.5, "repeated values not discarded");
    for (const NumericHistoryBucket &bucket : accumulator.result().buckets)
        check(bucket.count == 0 || bucket.value(HistoryAggregation::Average) == 67.5,
              "paged bucket average");

    const char *terminal = "<https://io.adafruit.com/data>";
    for (const char *bad : {"{}", "[", "[{\"value\":\"1\"}]",
                            "[{\"id\":\"a\",\"value\":\"1\",\"created_at\":\"bad\"}]"})
    {
        check(accumulator.begin(request, end), "reset malformed fixture");
        std::string json(bad);
        check(!readNumericHistoryPage(json, terminal, accumulator, cursor),
              "malformed page rejected");
    }
    std::string empty = "[]";
    check(readNumericHistoryPage(empty, terminal, accumulator, cursor),
          "empty complete window is valid");
    check(!readNumericHistoryPage(empty,
          "<https://io.adafruit.com/data?before=a>;rel=\"next\"", accumulator, cursor),
          "empty nonterminal page rejected");
    std::string unordered = "[" + record(1, end - 20, "1") + "," +
        record(2, end - 10, "2") + "]";
    check(!readNumericHistoryPage(unordered, terminal, accumulator, cursor),
          "out-of-order page rejected");
    std::string fullPage = "[";
    for (int i = 0; i < 100; ++i)
    {
        if (i)
            fullPage += ",";
        fullPage += record(i, end - i - 1, "10");
    }
    fullPage += "]";
    check(!readNumericHistoryPage(fullPage, "", accumulator, cursor),
          "full page without pagination metadata is not treated as complete");
    std::string oversized = fullPage.substr(0, fullPage.size() - 1) + "," +
        record(101, end - 101, "10") + "]";
    check(!readNumericHistoryPage(oversized, terminal, accumulator, cursor),
          "oversized page rejected");
    check(accumulator.begin(request, end), "empty-boundary fixture");
    check(accumulator.add("end", timestamp(end).c_str(), "99"), "exclusive end ignored");
    check(accumulator.result().sampleCount == 0, "end timestamp excluded");
    check(!accumulator.add("bad", "2026-02-30T12:00:00Z", "1"),
          "bad timestamp is an error, not an invented gap");
    check(accumulator.begin(request, end), "deduplication storage bound");
    const std::string sameTime = timestamp(end - 1);
    for (int i = 0; i < 1000; ++i)
        check(accumulator.add(std::to_string(i).c_str(), sameTime.c_str(), "10"),
              "bounded frontier records accepted");
    check(!accumulator.add("over-limit", sameTime.c_str(), "10"),
          "frontier storage limit is an explicit failure");

    std::printf("PASS: %zu numeric history checks\n", checks);
    return 0;
}
