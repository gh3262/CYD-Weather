#ifndef WEATHER_PAGE_VALUES_H
#define WEATHER_PAGE_VALUES_H

#include <ArduinoJson.h>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

template <size_t N>
using WeatherPageValues = std::array<std::array<char, 64>, N>;

// Numeric columns accept both JSON numbers and the producer's numeric strings.
template <size_t N>
inline const char *parseWeatherPageValues(const char *json,
                                         WeatherPageValues<N> &values,
                                         size_t columns, unsigned numericColumns)
{
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, json);
    if (error)
        return error.c_str();
    if (!document.is<JsonArray>() || document.size() != N)
        return "incorrect page array length";
    WeatherPageValues<N> parsed = {};
    for (size_t i = 0; i < N; ++i)
    {
        const JsonVariantConst value = document[i];
        const bool numeric = (numericColumns & (1U << (i % columns))) != 0;
        if (value.is<const char *>())
        {
            const char *text = value.as<const char *>();
            while (std::isspace(static_cast<unsigned char>(*text)))
                ++text;
            size_t length = std::strlen(text);
            while (length && std::isspace(static_cast<unsigned char>(text[length - 1])))
                --length;
            if (length >= parsed[i].size())
                return "page value is too long";
            std::memcpy(parsed[i].data(), text, length);
            if (numeric)
            {
                char *end;
                const double number = std::strtod(parsed[i].data(), &end);
                if (end == parsed[i].data() || *end || !std::isfinite(number))
                    return "expected a finite numeric page value";
            }
        }
        else if (numeric && value.is<double>() && std::isfinite(value.as<double>()))
        {
            const size_t length = serializeJson(value, parsed[i].data(), parsed[i].size());
            if (!length || length >= parsed[i].size())
                return "page number could not be formatted";
        }
        else
            return "incorrect page value type";
    }
    values = parsed;
    return nullptr;
}

using WindPageValues = WeatherPageValues<18>;
using BarometerPageValues = WeatherPageValues<12>;
using AstronomicalPageValues = WeatherPageValues<5>;

inline const char *parseWindPageValues(const char *json, WindPageValues &values)
{
    return parseWeatherPageValues(json, values, 3, 6);
}

inline const char *parseBarometerPageValues(const char *json, BarometerPageValues &values)
{
    return parseWeatherPageValues(json, values, 2, 1);
}

inline bool parseAstronomicalClock(const char *text, int &minutes)
{
    int hour, minute, consumed = 0;
    char period[3] = {};
    if (std::sscanf(text, "%d:%d %2s%n", &hour, &minute, period, &consumed) != 3 ||
        text[consumed] || hour < 1 || hour > 12 || minute < 0 || minute > 59 ||
        (std::strcmp(period, "AM") && std::strcmp(period, "PM")))
        return false;
    minutes = (hour % 12 + (std::strcmp(period, "PM") == 0 ? 12 : 0)) * 60 + minute;
    return true;
}

inline const char *parseAstronomicalPageValues(const char *json,
                                              AstronomicalPageValues &values,
                                              int &daylightMinutes)
{
    AstronomicalPageValues parsed;
    const char *error = parseWeatherPageValues(json, parsed, 1, 0);
    if (error)
        return error;
    int sunrise, sunset;
    if (!parseAstronomicalClock(parsed[0].data(), sunrise) ||
        !parseAstronomicalClock(parsed[1].data(), sunset))
        return "expected sunrise/sunset in h:mm AM/PM format";
    daylightMinutes = (sunset - sunrise + 1440) % 1440;
    values = parsed;
    return nullptr;
}

#endif
