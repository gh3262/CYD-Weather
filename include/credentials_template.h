// Copy to credentials.h (gitignored) and fill in your values.
#ifndef CREDENTIALS_H
#define CREDENTIALS_H

#include <stddef.h>

struct WiFiCredential
{
    const char *ssid;
    const char *password;
};

const WiFiCredential wifiNetworks[] =
{
    {"YOUR_SSID_1", "YOUR_PASSWORD_1"},
    {"YOUR_SSID_2", "YOUR_PASSWORD_2"}
};

const size_t WIFI_NETWORK_COUNT =
    sizeof(wifiNetworks) / sizeof(wifiNetworks[0]);

#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.nist.gov"
 
#define TZ_STRING "add time zone string here"

#define AIO_USERNAME "YOUR_AIO_USERNAME"
#define AIO_KEY "YOUR_AIO_KEY"

#endif
