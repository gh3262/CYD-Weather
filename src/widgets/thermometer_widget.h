#ifndef THERMOMETER_WIDGET_H
#define THERMOMETER_WIDGET_H

#include <Arduino.h>

struct ThermometerWidgetOptions
{
    int x;
    int y;
    int width;
    int height;
    float minimumTemperature;
    float maximumTemperature;
    float temperature;
    bool hasApparentTemperature;
    float apparentTemperature;
    uint16_t backgroundColor;
    uint16_t outlineColor;
    uint16_t tickColor;
    uint16_t freezingColor;
    uint16_t temperatureColor;
    uint16_t apparentColor;
    uint16_t labelColor;

    ThermometerWidgetOptions();
};

bool drawThermometerWidget(const ThermometerWidgetOptions &options);

#endif
