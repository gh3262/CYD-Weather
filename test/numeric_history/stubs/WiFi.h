#ifndef HISTORY_TEST_WIFI_H
#define HISTORY_TEST_WIFI_H
static const int WL_CONNECTED = 3;
struct TestWiFi
{
    int status() const { return WL_CONNECTED; }
};
extern TestWiFi WiFi;
#endif
