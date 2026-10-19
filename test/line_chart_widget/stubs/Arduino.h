#ifndef CHART_TEST_ARDUINO_H
#define CHART_TEST_ARDUINO_H

#include <stddef.h>
#include <stdint.h>

struct RecordingSerial
{
    size_t errors = 0;
    void println(const char *) { ++errors; }
};

extern RecordingSerial Serial;

#endif
