#ifndef NUMERIC_HISTORY_PAGE_H
#define NUMERIC_HISTORY_PAGE_H

#include <ArduinoJson.h>
#include "numeric_history.h"

static const size_t NUMERIC_HISTORY_PAGE_SIZE = 100;

// Shared by the HTTP worker and native fixtures; input may be a stream or JSON.
template <typename Input>
bool readNumericHistoryPage(Input &input, const char *link,
                            NumericHistoryAccumulator &accumulator,
                            std::string &nextCursor)
{
    JsonDocument filter;
    filter[0]["id"] = true;
    filter[0]["created_at"] = true;
    filter[0]["value"] = true;
    JsonDocument document;
    const DeserializationError error = deserializeJson(
        document, input, DeserializationOption::Filter(filter));
    if (error || !document.is<JsonArray>() ||
        document.size() > NUMERIC_HISTORY_PAGE_SIZE ||
        !historyNextCursor(link, nextCursor) ||
        (document.size() == NUMERIC_HISTORY_PAGE_SIZE && (!link || !*link)) ||
        (document.size() == 0 && !nextCursor.empty()))
        return false;

    int64_t previousTime = INT64_MAX;
    for (JsonVariant entry : document.as<JsonArray>())
    {
        int64_t timestamp;
        if (!entry["id"].is<const char *>() ||
            !entry["created_at"].is<const char *>() ||
            !entry["value"].is<const char *>() ||
            !parseHistoryTimestamp(entry["created_at"], timestamp) ||
            timestamp > previousTime ||
            !accumulator.add(entry["id"], entry["created_at"], entry["value"]))
            return false;
        previousTime = timestamp;
    }
    return true;
}

#endif
