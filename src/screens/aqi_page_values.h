#ifndef AQI_PAGE_VALUES_H
#define AQI_PAGE_VALUES_H

#include <ArduinoJson.h>
#include "adafruitio/numeric_history.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

inline bool formatAqiTimestamp(const char *text, char (&formatted)[17])
{
    if (!text)
        return false;
    const size_t length = std::strlen(text);
    if (length == 25)
    {
        if ((text[19] != '+' && text[19] != '-') || text[22] != ':')
            return false;
        for (size_t i : {20U, 21U, 23U, 24U})
            if (!std::isdigit(static_cast<unsigned char>(text[i])))
                return false;
        if ((text[20] - '0') * 10 + text[21] - '0' > 23 ||
            (text[23] - '0') * 10 + text[24] - '0' > 59)
            return false;
    }
    else if (length != 20 || text[19] != 'Z')
        return false;

    // Validate the calendar fields without changing the feed's wall-clock time.
    char calendar[21];
    std::snprintf(calendar, sizeof(calendar), "%.19sZ", text);
    int64_t milliseconds;
    if (!parseHistoryTimestamp(calendar, milliseconds))
        return false;
    std::snprintf(formatted, sizeof(formatted), "%.5s %.2s/%.2s/%.4s",
                  text + 11, text + 5, text + 8, text);
    return true;
}

inline const char *validateAqiPageValues(const JsonDocument &document)
{
    if (!document.is<JsonArrayConst>() || document.size() < 10)
        return "expected at least ten AQI page values";
    for (JsonVariantConst value : document.as<JsonArrayConst>())
        if (!value.is<const char *>())
            return "expected AQI page strings";
    char timestamp[17];
    if (!formatAqiTimestamp(document[3].as<const char *>(), timestamp))
        return "expected an ISO 8601 AQI timestamp";
    for (size_t offset : {0U, 4U, 7U})
    {
        const char *text = document[offset].as<const char *>();
        char *end;
        const double number = std::strtod(text, &end);
        const bool parsed = end != text;
        while (std::isspace(static_cast<unsigned char>(*end)))
            ++end;
        if (!parsed || *end || !std::isfinite(number) || number < 0)
            return "expected a finite nonnegative AQI";
        const char *color = document[offset + 2].as<const char *>();
        if (std::strlen(color) != 7 || color[0] != '#')
            return "expected an AQI color in #RRGGBB format";
        for (size_t i = 1; i < 7; ++i)
            if (!std::isxdigit(static_cast<unsigned char>(color[i])))
                return "expected an AQI color in #RRGGBB format";
    }
    return nullptr;
}

#endif
