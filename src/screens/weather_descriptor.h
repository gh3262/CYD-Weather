#ifndef WEATHER_DESCRIPTOR_H
#define WEATHER_DESCRIPTOR_H

#include <cmath>

constexpr bool validWeatherPressure(float pressure)
{
    return std::isfinite(pressure) && pressure > 0.0f;
}

// Trend is forecast pressure minus current pressure, in hPa.
constexpr const char *weatherDescriptor(float pressure, float forecastPressure)
{
    return !validWeatherPressure(pressure) || !validWeatherPressure(forecastPressure) ? "--"
        : pressure < 995.0f && forecastPressure - pressure < -2.0f ? "Stormy"
        : pressure < 1005.0f && forecastPressure - pressure > 2.0f ? "Clearing"
        : forecastPressure - pressure > 3.0f ? "Improving"
        : forecastPressure - pressure < -3.0f ? "Unsettled"
        : pressure > 1025.0f ? "Fair"
        : "Good";
}

#endif
