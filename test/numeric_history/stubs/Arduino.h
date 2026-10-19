#ifndef HISTORY_TEST_ARDUINO_H
#define HISTORY_TEST_ARDUINO_H

#include <algorithm>
#include <stdint.h>
#include <stddef.h>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <string>
#include <type_traits>

using std::min;

class String
{
public:
    String() = default;
    String(const char *text) : _text(text ? text : "") {}
    String(const std::string &text) : _text(text) {}
    template <typename Number,
              typename std::enable_if<std::is_arithmetic<Number>::value, int>::type = 0>
    String(Number number) : _text(std::to_string(number)) {}
    const char *c_str() const { return _text.c_str(); }
    bool isEmpty() const { return _text.empty(); }
    long toInt() const { return std::strtol(c_str(), nullptr, 10); }
    String &operator+=(const String &other) { _text += other._text; return *this; }
    friend String operator+(const String &a, const String &b)
    {
        return String(a._text + b._text);
    }
    bool operator==(const String &other) const { return _text == other._text; }
    bool operator!=(const String &other) const { return !(*this == other); }

private:
    std::string _text;
};

struct TestSerial
{
    void println(const char *) {}
    template <typename... Args> void printf(const char *, Args...) {}
};
extern TestSerial Serial;
extern uint32_t testMillis;
inline uint32_t millis() { return testMillis; }
typedef void *SemaphoreHandle_t;
typedef void *TaskHandle_t;
typedef int BaseType_t;
static const int pdPASS = 1;
static const uint32_t portMAX_DELAY = UINT32_MAX;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return reinterpret_cast<void *>(1); }
inline void xSemaphoreTake(SemaphoreHandle_t, uint32_t) {}
inline void xSemaphoreGive(SemaphoreHandle_t) {}
inline void vSemaphoreDelete(SemaphoreHandle_t) {}
inline size_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 4096; }
inline int xTaskCreatePinnedToCore(void (*)(void *), const char *, int, void *, int,
                                    TaskHandle_t *task, int)
{
    *task = reinterpret_cast<void *>(1);
    return pdPASS;
}
inline uint32_t pdMS_TO_TICKS(uint32_t value) { return value; }
inline void vTaskDelay(uint32_t) {}
#if defined(_WIN32)
inline struct tm *gmtime_r(const time_t *value, struct tm *result)
{
    const struct tm *utc = std::gmtime(value);
    if (!utc)
        return nullptr;
    *result = *utc;
    return result;
}
#endif

#endif
