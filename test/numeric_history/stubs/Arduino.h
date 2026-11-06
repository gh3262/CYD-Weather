#ifndef HISTORY_TEST_ARDUINO_H
#define HISTORY_TEST_ARDUINO_H

#include <algorithm>
#include <stdint.h>
#include <stddef.h>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <type_traits>

using std::min;

class String
{
public:
    String() = default;
    String(const String &) = default;
    String(String &&) = default;
    String &operator=(String &&) = default;
    String &operator=(const String &other)
    {
        _text = other._text.size() <= copyLimit() ? other._text : "";
        return *this;
    }
    static size_t &copyLimit()
    {
        static size_t limit = SIZE_MAX;
        return limit;
    }
    String(const char *text) : _text(text ? text : "") {}
    String(const std::string &text) : _text(text) {}
    String(double value, unsigned decimals)
    {
        char buffer[96];
        std::snprintf(buffer, sizeof(buffer), "%.*f", static_cast<int>(decimals), value);
        _text = buffer;
    }
    template <typename Number,
              typename std::enable_if<std::is_arithmetic<Number>::value, int>::type = 0>
    String(Number number) : _text(std::to_string(number)) {}
    const char *c_str() const { return _text.c_str(); }
    bool isEmpty() const { return _text.empty(); }
    size_t length() const { return _text.length(); }
    void trim()
    {
        const size_t first = _text.find_first_not_of(" \t\r\n");
        _text = first == std::string::npos ? "" :
            _text.substr(first, _text.find_last_not_of(" \t\r\n") - first + 1);
    }
    bool endsWith(const char *suffix) const
    {
        const size_t length = std::strlen(suffix);
        return _text.size() >= length &&
            _text.compare(_text.size() - length, length, suffix) == 0;
    }
    String substring(size_t first, size_t last) const
    {
        return String(_text.substr(first, last - first));
    }
    String substring(size_t first) const { return String(_text.substr(first)); }
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
inline struct tm *localtime_r(const time_t *value, struct tm *result)
{
    const struct tm *local = std::localtime(value);
    if (!local)
        return nullptr;
    *result = *local;
    return result;
}
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
