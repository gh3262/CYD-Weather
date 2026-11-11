#ifndef WIND_SNAPSHOT_TEST_WIFI_H
#define WIND_SNAPSHOT_TEST_WIFI_H
static const int WL_CONNECTED = 3;
struct TestWiFi
{
    bool connected = true;
    int status() const;
};
extern TestWiFi WiFi;
#endif
