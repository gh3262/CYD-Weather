#ifndef TEMPERATURE_PAGE_VALUES_H
#define TEMPERATURE_PAGE_VALUES_H

#include <ArduinoJson.h>
#include <array>
#include <math.h>

using TemperaturePageValues = std::array<float, 6>;

// Validate the whole snapshot before replacing any values.
inline const char *parseTemperaturePageValues(const char *json,
                                              TemperaturePageValues &values)
{
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, json);
    if (error)
        return error.c_str();
    if (!document.is<JsonArray>() || document.size() != values.size())
        return "expected an array of six temperatures";

    TemperaturePageValues parsed;
    for (size_t i = 0; i < parsed.size(); ++i)
    {
        const JsonVariantConst value = document[i];
        if (!value.is<float>())
            return "expected six numeric temperatures";
        parsed[i] = value.as<float>();
        if (!isfinite(parsed[i]))
            return "temperature is not finite";
    }
    values = parsed;
    return nullptr;
}

#endif
