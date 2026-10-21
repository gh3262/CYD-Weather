#include "screens/weather_descriptor.h"

#include <limits>

constexpr bool sameText(const char *left, const char *right)
{
    return *left == *right && (*left == '\0' || sameText(left + 1, right + 1));
}

static_assert(sameText(weatherDescriptor(994, 991), "Stormy"), "Low and falling");
static_assert(sameText(weatherDescriptor(994, 992), "Nice"), "Storm threshold is strict");
static_assert(sameText(weatherDescriptor(995, 992), "Nice"), "Low pressure threshold is strict");
static_assert(sameText(weatherDescriptor(995, 988), "Unsettled"), "Not below 995");
static_assert(sameText(weatherDescriptor(996, 1000), "Clearing"), "Clearing precedes improving");
static_assert(sameText(weatherDescriptor(1004, 1007), "Clearing"), "Below clearing pressure limit");
static_assert(sameText(weatherDescriptor(1004, 1006), "Nice"), "Clearing trend threshold is strict");
static_assert(sameText(weatherDescriptor(1005, 1008), "Nice"), "Clearing pressure limit is strict");
static_assert(sameText(weatherDescriptor(1010, 1014), "Improving"), "Rising forecast pressure");
static_assert(sameText(weatherDescriptor(1010, 1013), "Nice"), "Rising threshold is strict");
static_assert(sameText(weatherDescriptor(1010, 1006), "Unsettled"), "Falling forecast pressure");
static_assert(sameText(weatherDescriptor(1010, 1007), "Nice"), "Falling threshold is strict");
static_assert(sameText(weatherDescriptor(1026, 1026), "Fair"), "High steady pressure");
static_assert(sameText(weatherDescriptor(1025, 1025), "Nice"), "High threshold is strict");
static_assert(sameText(weatherDescriptor(1027, 1031), "Improving"), "Trend precedes fair");
static_assert(sameText(weatherDescriptor(1027, 1023), "Unsettled"), "Falling high pressure");
static_assert(sameText(weatherDescriptor(1015, 1015), "Nice"), "Normal steady pressure");
static_assert(sameText(weatherDescriptor(0, 1015), "--"), "Missing current pressure");
static_assert(sameText(weatherDescriptor(1015, -1), "--"), "Invalid forecast pressure");
static_assert(sameText(weatherDescriptor(std::numeric_limits<float>::quiet_NaN(), 1015), "--"), "NaN");
static_assert(sameText(weatherDescriptor(1015, std::numeric_limits<float>::infinity()), "--"), "Infinity");

int main()
{
    return 0;
}
