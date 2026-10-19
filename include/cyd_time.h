#ifndef CYD_TIME_H
#define CYD_TIME_H

#include <time.h>
#include "esp_sntp.h"
#include "credentials.h"

// Start time text; empty until the first successful sync
inline char* synchronizedStartDateTime()
{
    static char startDateTime[32] = "";
    return startDateTime;
}

// Set by the SNTP callback each time a real NTP reply updates the clock
inline volatile bool& ntpReplyReceived()
{
    static volatile bool received = false;
    return received;
}

inline void onNtpSync(struct timeval *)
{
    ntpReplyReceived() = true;
}

// Starts an NTP request and returns immediately; ntpReplyReceived() turns
// true when the reply arrives, then call finishTimeSync()
inline void startTimeSync()
{
    ntpReplyReceived() = false;
    sntp_set_time_sync_notification_cb(onNtpSync);

    configTime(
        0,
        0,
        NTP_SERVER_1,
        NTP_SERVER_2);

    setenv("TZ", TZ_STRING, 1);
    tzset();
}

// Call once ntpReplyReceived() is true
inline bool finishTimeSync()
{
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo, 0))
        return false;

    if (synchronizedStartDateTime()[0] == '\0')
    {
        char buffer[32];
        if (strftime(buffer, sizeof(buffer), "%I:%M %p  %m/%d/%Y", &timeinfo) == 0)
            return false;

        strcpy(synchronizedStartDateTime(), buffer);
    }

    return true;
}

// Blocking version, for startup only
inline bool syncTime()
{
    startTimeSync();

    // getLocalTime() alone returns at once if the clock was already set, so
    // wait for an actual NTP reply to know this sync really happened
    const unsigned long start = millis();
    while (!ntpReplyReceived())
    {
        if (millis() - start > 10000)
            return false;

        delay(50);
    }

    return finishTimeSync();
}

inline const char* startDateTimeString()
{
    if (synchronizedStartDateTime()[0] == '\0')
        return "Time unavailable";

    return synchronizedStartDateTime();
}

// The returned pointers are to static buffers: use them right away
inline const char* currentTimeString()
{
    static char buffer[13];
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo, 0))
        return "No Time";

    strftime(buffer, sizeof(buffer), "%I:%M %p", &timeinfo);
    return buffer;
}

inline const char* currentDateString()
{
    static char buffer[32];
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo, 0))
        return "No Date";

    strftime(buffer, sizeof(buffer), "%m/%d/%Y", &timeinfo);
    return buffer;
}

// ====================================================
// UTC Timestamp Feeds ("2026-10-20T20:21:25Z")
// ====================================================

// Days since 1970-01-01 for a civil date (proleptic Gregorian); avoids
// mktime(), which would treat the fields as local time
inline long daysFromCivil(int year, unsigned month, unsigned day)
{
    year -= month <= 2;
    const long era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + static_cast<long>(dayOfEra) - 719468;
}

// Parses an ISO 8601 UTC timestamp ending in 'Z'; returns false if malformed
inline bool parseUtcTimestamp(const char* text, time_t& result)
{
    if (text == nullptr)
        return false;

    int year, month, day, hour, minute, second;
    char zone = '\0';
    int consumed = 0;

    if (sscanf(text, "%4d-%2d-%2dT%2d:%2d:%2d%c%n",
               &year, &month, &day, &hour, &minute, &second, &zone, &consumed) != 7)
        return false;

    if (zone != 'Z' || text[consumed] != '\0')
        return false;

    if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || minute > 59 || second > 60)
        return false;

    const long days = daysFromCivil(year, month, day);
    result = static_cast<time_t>(days * 86400L + hour * 3600L + minute * 60L + second);
    return true;
}

// Converts a UTC timestamp feed value to local time (TZ_STRING, e.g. US
// Central with DST) using a strftime format. Returns a static buffer: use it
// right away. Returns "--" if the value can't be parsed.
inline const char* formatUtcTimestampLocal(const char* text, const char* format = "%I:%M %p")
{
    static char buffer[32];
    time_t utc;
    struct tm local;

    if (!parseUtcTimestamp(text, utc) || localtime_r(&utc, &local) == nullptr)
        return "--";

    if (strftime(buffer, sizeof(buffer), format, &local) == 0)
        return "--";

    return buffer;
}
#endif