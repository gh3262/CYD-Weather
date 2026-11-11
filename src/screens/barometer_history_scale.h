#ifndef BAROMETER_HISTORY_SCALE_H
#define BAROMETER_HISTORY_SCALE_H

#include "screens/history_chart_page.h"

// Chooses the finest scale with at most eight Y labels so large swings fit.
inline HistoryChartScale pressureHistoryScale(double minimumY, double maximumY)
{
    static const HistoryChartScale SCALES[] = {
        {0.5, 1.0}, {1.0, 2.0}, {1.0, 5.0}, {5.0, 10.0}, {10.0, 20.0}, {10.0, 50.0},
    };
    const double range = maximumY - minimumY;
    for (const HistoryChartScale &scale : SCALES)
    {
        if (range / scale.labelInterval <= 8.0)
            return scale;
    }
    return {50.0, 100.0};
}

#endif
