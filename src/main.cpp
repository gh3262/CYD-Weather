#include <Arduino.h>
#include <WiFi.h>
#include "app.h"
#include "credentials.h"

AppState state = {SCREEN_HOME, PING_NOT_RUN, PING_NOT_RUN};

// Connection
bool connectWiFi();

// Diagnostics
void colorTest();

void setup()
{
    Serial.begin(115200);

    if (!initCYD())
    {
        while (true)
        {
        }
    }

    showSplash("CYD Framework");

    if (!soundInit())
        Serial.println("Sound initialization failed.");

    const bool wifiOk = connectWiFi();

    if (wifiOk && !syncTime())
        Serial.println("Time synchronization failed; start time unavailable.");

    // Played after the blocking boot steps so loop() can sequence the tones
    playSound(wifiOk ? SOUND_SUCCESS : SOUND_ERROR);

    //colorTest();
    // while (true);
    // while (true);

    drawCurrentScreen();
}

static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
static const unsigned long WIFI_RETRY_INTERVAL_MS = 10000;

struct StrongestKnownNetwork
{
    size_t credentialIndex;
    int32_t rssi;
    int32_t channel;
    uint8_t bssid[6];
};

static void showSplashStatus(const char* text)
{
    gfx->fillRect(0, 120, SCREEN_WIDTH, 40, COLOR_BACKGROUND);
    centerText(text, 130, COLOR_TEXT, 1);
}

static bool findStrongestKnownNetwork(StrongestKnownNetwork &strongest)
{
    const int16_t found = WiFi.scanNetworks(false, false, false, 300);
    if (found < 0)
    {
        Serial.printf("WiFi scan failed with error %d.\n", found);
        WiFi.scanDelete();
        return false;
    }

    bool haveKnownNetwork = false;
    for (int16_t networkIndex = 0; networkIndex < found; ++networkIndex)
    {
        const String scannedSsid = WiFi.SSID(networkIndex);
        for (size_t credentialIndex = 0;
             credentialIndex < WIFI_NETWORK_COUNT;
             ++credentialIndex)
        {
            if (scannedSsid != wifiNetworks[credentialIndex].ssid)
                continue;

            const int32_t rssi = WiFi.RSSI(networkIndex);
            if (haveKnownNetwork && rssi <= strongest.rssi)
                break;

            uint8_t *bssid = WiFi.BSSID(networkIndex);
            if (!bssid)
                break;

            strongest.credentialIndex = credentialIndex;
            strongest.rssi = rssi;
            strongest.channel = WiFi.channel(networkIndex);
            memcpy(strongest.bssid, bssid, sizeof(strongest.bssid));
            haveKnownNetwork = true;
            break;
        }
    }

    WiFi.scanDelete();
    return haveKnownNetwork;
}

static bool beginStrongestKnownConnection()
{
    StrongestKnownNetwork strongest;
    if (!findStrongestKnownNetwork(strongest))
    {
        Serial.println("WiFi: no configured network found in scan.");
        return false;
    }

    const WiFiCredential &credentials = wifiNetworks[strongest.credentialIndex];
    Serial.printf("WiFi: connecting to strongest known network (%ld dBm).\n",
                  static_cast<long>(strongest.rssi));
    WiFi.begin(
        credentials.ssid,
        credentials.password,
        strongest.channel,
        strongest.bssid);
    return true;
}

bool connectWiFi()
{
    networkConnecting();
    showSplashStatus("Scanning for known WiFi...");

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    delay(100);

    if (!beginStrongestKnownConnection())
    {
        networkDisconnected();
        showSplashStatus("No known WiFi found");
        return false;
    }

    showSplashStatus("Connecting to strongest network...");

    const unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS)
        {
            networkDisconnected();
            showSplashStatus("WiFi connection failed");
            Serial.println();
            Serial.println("WiFi connection timed out");
            WiFi.disconnect(false, false);
            delay(1500);
            return false;
        }

        delay(500);
        Serial.print(".");
    }

    networkConnected();

    Serial.println();
    Serial.println("WiFi Connected");
    return true;
}

// Called from loop(): keeps the LED honest and retries without blocking
static void maintainWiFi()
{
    static bool wasConnected = true;
    // Distinguishes a real drop from the first pass after a failed boot connect
    static bool hadLink = (WiFi.status() == WL_CONNECTED);
    static unsigned long lastRetry = 0;

    const bool connected = (WiFi.status() == WL_CONNECTED);

    if (connected)
    {
        if (!wasConnected)
        {
            Serial.println("WiFi reconnected");
            networkSignalLED(WiFi.RSSI());
            playSound(SOUND_SUCCESS);

            // Time was never set if the boot-time sync was skipped or failed
            if (synchronizedStartDateTime()[0] == '\0')
                startNtpSync();

            drawCurrentScreen();
        }
    }
    else
    {
        if (wasConnected)
        {
            Serial.println("WiFi lost");
            networkDisconnected();
            if (hadLink)
                playSound(SOUND_ERROR);
            lastRetry = millis();
        }
        else if (millis() - lastRetry > WIFI_RETRY_INTERVAL_MS)
        {
            lastRetry = millis();
            beginStrongestKnownConnection();
        }
    }

    wasConnected = connected;
    if (connected)
        hadLink = true;
}

// One entry per Screen, in enum order
struct ScreenDef
{
    void (*draw)();
    void (*handleTouch)(int x, int y);
    void (*refreshResults)();   // nullptr when the screen has no live result rows
    bool hasHomeButton;
};

static const ScreenDef screens[] = {
    /* SCREEN_HOME        */ { drawHomeScreen,       handleHomeTouch,       nullptr,                    false },
    /* SCREEN_WIFI_STATUS */ { drawWifiStatusScreen, handleWifiStatusTouch, refreshWifiStatusResults,   true  },
    /* SCREEN_SYSTEM      */ { drawSystemScreen,     handleSystemTouch,     nullptr,                    true  },
    /* SCREEN_TOOLS       */ { drawToolsScreen,      handleToolsTouch,      refreshToolsResults,        true  },
    /* SCREEN_TOUCH_CALIBRATION */ { drawTouchCalibrationScreen, handleTouchCalibrationTouch, nullptr, false },
};

static_assert(sizeof(screens) / sizeof(screens[0]) == SCREEN_COUNT,
              "screens[] must have one entry per Screen");

void drawCurrentScreen()
{
    screens[state.currentScreen].draw();
}

void refreshCurrentScreenResults()
{
    const ScreenDef &screen = screens[state.currentScreen];

    if (screen.refreshResults)
        screen.refreshResults();
}

void colorTest()
{
    gfx->fillRect(0,   0, 80, 80, CYD_BLACK);
    gfx->fillRect(80,  0, 80, 80, CYD_WHITE);
    gfx->fillRect(160, 0, 80, 80, CYD_RED);
    gfx->fillRect(240, 0, 80, 80, CYD_GREEN);

    gfx->fillRect(0,   80, 80, 80, CYD_BLUE);
    gfx->fillRect(80,  80, 80, 80, CYD_YELLOW);
    gfx->fillRect(160, 80, 80, 80, CYD_CYAN);
    gfx->fillRect(240, 80, 80, 80, CYD_MAGENTA);
}

static void handleTouch(int x, int y)
{
    const ScreenDef &screen = screens[state.currentScreen];

    // Home button (present on every screen except Home)
    if (screen.hasHomeButton && navPressed4(x, y))
    {
        enterHomeScreen();
        return;
    }

    screen.handleTouch(x, y);
}
void loop()
{
    int x, y;
    static bool touchArmed = true;
    static unsigned long lastTouchMs = 0;

    maintainWiFi();
    soundUpdate();
    updateBackgroundJobs();

    static int lastMinute = -1;

    struct tm timeinfo;

    // Zero timeout: don't stall the UI when the clock was never set
    if (getLocalTime(&timeinfo, 0))
    {
        if (timeinfo.tm_min != lastMinute)
        {
            drawClock();
            if (state.currentScreen == SCREEN_HOME)
                refreshHomeTime();

            lastMinute = timeinfo.tm_min;
        }
    }

    if (getTouch(x, y))
    {
        lastTouchMs = millis();

        // Act once per press; ignore the finger while it stays down
        if (touchArmed)
        {
            touchArmed = false;

            Serial.print("Touch: ");
            Serial.print(x);
            Serial.print(",");
            Serial.println(y);

            handleTouch(x, y);
        }
    }
    else if (!touchArmed && millis() - lastTouchMs > TOUCH_RELEASE_MS)
    {
        touchArmed = true;
    }
}
