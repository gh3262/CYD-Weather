#ifndef PRECIPITATION_PAGE_VALUES_H
#define PRECIPITATION_PAGE_VALUES_H

#include <ArduinoJson.h>
#include <array>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const size_t PRECIPITATION_PAGE_VALUE_COUNT = 21;
static const size_t PRECIPITATION_PAGE_VALUE_LENGTH = 64;

using PrecipitationPageValues =
    std::array<std::array<char, PRECIPITATION_PAGE_VALUE_LENGTH>,
               PRECIPITATION_PAGE_VALUE_COUNT>;

inline const char *parsePrecipitationPageValues(
    const char *json, PrecipitationPageValues &values)
{
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, json);
    if (error)
        return error.c_str();
    if (!document.is<JsonArray>() ||
        document.size() != PRECIPITATION_PAGE_VALUE_COUNT)
        return "expected an array of 21 precipitation values";

    PrecipitationPageValues parsed = {};
    for (size_t i = 0; i < parsed.size(); ++i)
    {
        const JsonVariantConst value = document[i];
        if (i % 3 == 1)
        {
            if (!value.is<const char *>())
                return "expected a precipitation type string";
            const char *text = value.as<const char *>();
            const size_t length = strlen(text);
            if (length >= parsed[i].size())
                return "precipitation type is too long";
            memcpy(parsed[i].data(), text, length + 1);
            continue;
        }

        size_t length = 0;
        if (value.is<float>())
        {
            if (!isfinite(value.as<float>()))
                return "precipitation value is not finite";
            length = serializeJson(value, parsed[i].data(), parsed[i].size());
        }
        else if (value.is<const char *>())
        {
            const char *text = value.as<const char *>();
            char *end = nullptr;
            const float number = strtof(text, &end);
            while (end && isspace(static_cast<unsigned char>(*end)))
                ++end;
            if (end == text || !end || *end != '\0' || !isfinite(number))
                return "expected numeric precipitation chance and amount values";

            while (isspace(static_cast<unsigned char>(*text)))
                ++text;
            length = strlen(text);
            while (length > 0 &&
                   isspace(static_cast<unsigned char>(text[length - 1])))
                --length;
            if (length >= parsed[i].size())
                return "precipitation number is too long";
            memcpy(parsed[i].data(), text, length);
            parsed[i][length] = '\0';
        }
        else
        {
            return "expected numeric precipitation chance and amount values";
        }

        if (length == 0 || length >= parsed[i].size())
        {
            if (value.is<const char *>() && length == 0)
                continue;
            return "precipitation number could not be formatted";
        }
    }

    values = parsed;
    return nullptr;
}

#endif
