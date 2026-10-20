#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include <WiFi.h>
#include "cyd.h"
#include "audio/sound_manager.h"

// ====================================================
// Application State
// ====================================================

enum Screen
{
    SCREEN_HOME,
    SCREEN_SNAPSHOT,
    SCREEN_SUMMARY,
    SCREEN_FORECAST,
    SCREEN_DETAILS,
    SCREEN_PRECIPITATION,
    SCREEN_BAROMETER,
    SCREEN_BAROMETER_HISTORY,
    SCREEN_WIND,
    SCREEN_WIND_HISTORY,
    SCREEN_TEMPERATURES,
    SCREEN_TEMPERATURE_HISTORY,
    SCREEN_ASTRONOMICAL,
    SCREEN_ALERTS,
    SCREEN_ALERT_DETAILS,
    SCREEN_FULL_ALERT,
    SCREEN_WIFI_STATUS,
    SCREEN_SYSTEM,
    SCREEN_TOOLS,
    SCREEN_TOUCH_CALIBRATION,
    SCREEN_COUNT   // keep last
};

struct AppState
{
    Screen currentScreen;
    int gatewayPingMs;
    int internetPingMs;
};

const int PING_RUNNING = -3;
const int PING_SLOW_MS = 100;
const int PING_NOT_RUN = -2;
const int PING_FAILED = -1;

const char* const INTERNET_PING_HOST = "8.8.8.8";

// Defined in main.cpp
extern AppState state;

// ====================================================
// Navigation (main.cpp)
// ====================================================

void drawCurrentScreen();
// Redraws only the result rows (pings, NTP status) of the visible screen
void refreshCurrentScreenResults();

// ====================================================
// Screens (src/screens/)
//   draw*   - render the screen
//   enter*  - make it the current screen and draw it
//   handle* - process a touch while it is the current screen
// ====================================================

// home_screen.cpp
void drawHomeScreen();
void enterHomeScreen();
void handleHomeTouch(int x, int y);
void refreshHomeTime();
void refreshHomeTemperature();
void refreshHomeScreen();

// snapshot_screen.cpp
void drawSnapshotScreen();
void enterSnapshotScreen();
void handleSnapshotTouch(int x, int y);
void refreshSnapshotResults();

// summary_screen.cpp
void drawSummaryScreen();
void enterSummaryScreen();
void handleSummaryTouch(int x, int y);
void refreshSummaryResults();

// alerts_screen.cpp
void drawAlertsScreen();
void enterAlertsScreen();
void handleAlertsTouch(int x, int y);
void refreshAlertsResults();
void enterAlertDetailsScreen();
void enterFullAlertScreen();

// forecast_screen.cpp
void drawForecastScreen();
void enterForecastScreen();
void handleForecastTouch(int x, int y);
void refreshForecastResults();

// details_screen.cpp
void drawDetailsScreen();
void enterDetailsScreen();
void handleDetailsTouch(int x, int y);
void refreshDetailsResults();

// precipitation_screen.cpp
void drawPrecipitationScreen();
void enterPrecipitationScreen();
void handlePrecipitationTouch(int x, int y);
void refreshPrecipitationResults();

// barometer_screen.cpp
void drawBarometerScreen();
void enterBarometerScreen();
void handleBarometerTouch(int x, int y);
void refreshBarometerResults();

// barometer_history_screen.cpp
void drawBarometerHistoryScreen();
void enterBarometerHistoryScreen();
void handleBarometerHistoryTouch(int x, int y);
void refreshBarometerHistoryResults();

// wind_screen.cpp
void drawWindScreen();
void enterWindScreen();
void handleWindTouch(int x, int y);
void refreshWindResults();

// wind_history_screen.cpp
void drawWindHistoryScreen();
void enterWindHistoryScreen();
void handleWindHistoryTouch(int x, int y);
void refreshWindHistoryResults();

// temperatures_screen.cpp
void drawTemperaturesScreen();
void enterTemperaturesScreen();
void handleTemperaturesTouch(int x, int y);
void refreshTemperaturesResults();

// temperature_history_screen.cpp
void drawTemperatureHistoryScreen();
void enterTemperatureHistoryScreen();
void handleTemperatureHistoryTouch(int x, int y);
void refreshTemperatureHistoryResults();

// astronomical_screen.cpp
void drawAstronomicalScreen();
void enterAstronomicalScreen();
void handleAstronomicalTouch(int x, int y);
void refreshAstronomicalResults();

// wifi_status_screen.cpp
void drawWifiStatusScreen();
void enterWifiStatusScreen();
void handleWifiStatusTouch(int x, int y);
void refreshWifiStatusResults();

// system_screen.cpp
void drawSystemScreen();
void enterSystemScreen();
void handleSystemTouch(int x, int y);

// tools_screen.cpp
void drawToolsScreen();
void enterToolsScreen();
void handleToolsTouch(int x, int y);
void refreshToolsResults();

// touch_calibration_screen.cpp
void drawTouchCalibrationScreen();
void enterTouchCalibrationScreen();
void handleTouchCalibrationTouch(int x, int y);

// Background jobs: each start*() returns at once, and
// updateBackgroundJobs() (called from loop) redraws when one finishes
void startPings();
void startNtpSync();
void updateBackgroundJobs();

#endif
