#include "thermometer_widget.h"

#include <math.h>

#include "app.h"

static const int THERMOMETER_MIN_WIDTH = 56;
static const int THERMOMETER_MIN_HEIGHT = 80;

ThermometerWidgetOptions::ThermometerWidgetOptions()
    : x(0),
      y(HEADER_HEIGHT),
      width(80),
      height(NAV_Y - HEADER_HEIGHT),
      minimumTemperature(0.0f),
      maximumTemperature(100.0f),
      temperature(50.0f),
      hasApparentTemperature(false),
      apparentTemperature(50.0f),
      backgroundColor(CYD_NAVY),
      outlineColor(CYD_WHITE),
      tickColor(CYD_WHITE),
      freezingColor(CYD_RED),
      temperatureColor(CYD_RED),
      apparentColor(CYD_CYAN),
      labelColor(CYD_WHITE)
{
}

static int temperatureToY(float temperature, float minimum, float maximum,
                          int top, int bottom)
{
    const float fraction = (temperature - minimum) / (maximum - minimum);
    return bottom - static_cast<int>(fraction * (bottom - top) + 0.5f);
}

static float clampTemperature(float temperature, float minimum, float maximum)
{
    if (temperature < minimum)
        return minimum;
    if (temperature > maximum)
        return maximum;
    return temperature;
}

bool drawThermometerWidget(const ThermometerWidgetOptions &options)
{
    if (options.width < THERMOMETER_MIN_WIDTH ||
        options.height < THERMOMETER_MIN_HEIGHT ||
        !isfinite(options.minimumTemperature) ||
        !isfinite(options.maximumTemperature) ||
        !isfinite(options.temperature) ||
        options.minimumTemperature >= options.maximumTemperature ||
        (options.hasApparentTemperature &&
         !isfinite(options.apparentTemperature)))
    {
        Serial.println("Thermometer widget: invalid size or temperature range.");
        return false;
    }

    gfx->fillRect(
        options.x, options.y, options.width, options.height,
        options.backgroundColor);

    const int labelRight = options.x + options.width * 2 / 3 - 8;
    const int tubeWidth = max(10, min(18, options.width / 5));
    const int tubeX = options.x + options.width * 2 / 3;
    const int top = options.y + 7;
    const int bulbRadius = max(8, min(options.width / 6, options.height / 10));
    const int bulbCenterX = tubeX + tubeWidth / 2;
    const int bulbCenterY = options.y + options.height - bulbRadius - 2;
    const int bottom = bulbCenterY;
    const int tubeHeight = bottom - top;

    if (tubeHeight < 20 || labelRight <= options.x)
    {
        Serial.println("Thermometer widget: bounds are too small for the scale.");
        return false;
    }

    gfx->drawRoundRect(
        tubeX, top, tubeWidth, tubeHeight,
        tubeWidth / 2, options.outlineColor);

    const int innerX = tubeX + 3;
    const int innerWidth = tubeWidth - 6;
    const int innerTop = top + 3;
    const int innerBottom = bottom - 2;
    const float actual = clampTemperature(
        options.temperature, options.minimumTemperature, options.maximumTemperature);
    const int actualY = temperatureToY(
        actual, options.minimumTemperature, options.maximumTemperature,
        innerTop, innerBottom);

    int actualWidth = innerWidth;
    int apparentX = innerX;
    int apparentWidth = 0;
    if (options.hasApparentTemperature && innerWidth >= 8)
    {
        apparentWidth = max(3, innerWidth / 3);
        actualWidth = innerWidth - apparentWidth;
        apparentX = innerX + actualWidth;
    }

    if (innerBottom > actualY)
    {
        gfx->fillRect(
            innerX, actualY, actualWidth, innerBottom - actualY,
            options.temperatureColor);
    }

    if (options.hasApparentTemperature && apparentWidth > 0)
    {
        const float apparent = clampTemperature(
            options.apparentTemperature,
            options.minimumTemperature,
            options.maximumTemperature);
        const int apparentY = temperatureToY(
            apparent, options.minimumTemperature, options.maximumTemperature,
            innerTop, innerBottom);
        if (innerBottom > apparentY)
        {
            gfx->fillRect(
                apparentX, apparentY, apparentWidth,
                innerBottom - apparentY, options.apparentColor);
        }
    }

    gfx->fillCircle(
        bulbCenterX, bulbCenterY, bulbRadius, options.temperatureColor);
    gfx->drawCircle(
        bulbCenterX, bulbCenterY, bulbRadius, options.outlineColor);

    const int tickStartX = tubeX - 5;
    const int tickEndX = tubeX;
    const float scaleHeight = static_cast<float>(bottom - top);
    const float temperatureRange =
        options.maximumTemperature - options.minimumTemperature;
    const int tickStep = max(
        10,
        static_cast<int>(ceilf((temperatureRange * 12.0f / scaleHeight) / 10.0f) * 10));
    gfx->setFont();
    gfx->setTextSize(1);
    for (int tick = static_cast<int>(
             ceilf(options.minimumTemperature / tickStep) * tickStep);
         tick <= options.maximumTemperature;
         tick += tickStep)
    {
        const int tickY = temperatureToY(
            static_cast<float>(tick), options.minimumTemperature,
            options.maximumTemperature, top + 2, bottom - 2);
        gfx->drawLine(tickStartX, tickY, tickEndX, tickY, options.tickColor);
        const String label(tick);
        int16_t labelX1, labelY1;
        uint16_t labelWidth, labelHeight;
        gfx->getTextBounds(
            label.c_str(), 0, 0, &labelX1, &labelY1, &labelWidth, &labelHeight);
        gfx->setTextColor(options.tickColor);
        gfx->setCursor(
            labelRight - (labelX1 + labelWidth), tickY - labelHeight / 2 - labelY1);
        gfx->print(label);
    }

    if (options.minimumTemperature <= 32.0f &&
        options.maximumTemperature >= 32.0f)
    {
        const int freezingY = temperatureToY(
            32.0f, options.minimumTemperature, options.maximumTemperature,
            top + 2, bottom - 2);
        gfx->drawLine(
            tickStartX - 2, freezingY, tickEndX, freezingY,
            options.freezingColor);
    }

    gfx->setTextColor(COLOR_TEXT);
    setDefaultFont();
    gfx->setTextSize(2);
    return true;
}
