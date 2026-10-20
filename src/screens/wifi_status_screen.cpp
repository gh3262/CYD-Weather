#include "app.h"

// Prints a single ping time using the same colors as the Tools page
static void printPingValue(int pingMs)
{
    if (pingMs >= 0)
    {
        gfx->setTextColor(pingMs > PING_SLOW_MS ? COLOR_WARNING : COLOR_OK);
        gfx->print(pingMs);
    }
    else
    {
        gfx->setTextColor(COLOR_ERROR);
        gfx->print("--");
    }
}

// Ping and health rows, redrawn on their own when results change
static void drawPingRows()
{
    setBodyFont();
    gfx->setTextSize(1);
    gfx->fillRect(0, 137, SCREEN_WIDTH, 53, COLOR_BACKGROUND);

    gfx->setCursor(10, 150);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("Ping: ");
    gfx->setTextColor(COLOR_TEXT);
    if (state.gatewayPingMs == PING_RUNNING || state.internetPingMs == PING_RUNNING)
    {
        gfx->print("Pinging...");
    }
    else if (state.gatewayPingMs == PING_NOT_RUN && state.internetPingMs == PING_NOT_RUN)
    {
        gfx->print("--");
    }
    else
    {
        printPingValue(state.gatewayPingMs);
        gfx->setTextColor(COLOR_TEXT);
        gfx->print(" / ");
        printPingValue(state.internetPingMs);
        gfx->setTextColor(COLOR_TEXT);
        gfx->print(" ms");
    }

    // Kept above NAV_Y so it doesn't collide with the buttons
    gfx->setCursor(10, 170);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("Health: ");
    gfx->setTextColor(COLOR_TEXT);

    if (state.internetPingMs == PING_NOT_RUN && state.gatewayPingMs == PING_NOT_RUN)
    {
        gfx->println("--");
    }
    else if (state.internetPingMs >= 0)
    {
        gfx->setTextColor(COLOR_OK);
        gfx->println("ONLINE");
    }
    else if (state.gatewayPingMs >= 0)
    {
        gfx->setTextColor(COLOR_WARNING);
        gfx->println("LOCAL ONLY");
    }
    else
    {
        gfx->setTextColor(COLOR_ERROR);
        gfx->println("OFFLINE");
    }

    gfx->setTextColor(COLOR_TEXT);
    setDefaultFont();
    gfx->setTextSize(2);
}

void refreshWifiStatusResults()
{
    drawPingRows();
}

void drawWifiStatusScreen()
{
    clearScreen(COLOR_BACKGROUND);

    titleBar(
        "WIFI STATUS",
        COLOR_HEADER_BG,
        COLOR_HEADER_TEXT);

    setBodyFont();
    gfx->setTextSize(1);

    gfx->setCursor(10, 50);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("SSID: ");
    gfx->setTextColor(COLOR_TEXT);
    gfx->println(WiFi.SSID());

    gfx->setCursor(10, 70);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("IP: ");
    gfx->setTextColor(COLOR_TEXT);
    gfx->println(WiFi.localIP());

    gfx->setCursor(10, 90);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("RSSI: ");

    const int rssi = WiFi.RSSI();
    if (rssi > -65)
        gfx->setTextColor(COLOR_OK);
    else if (rssi > -80)
        gfx->setTextColor(COLOR_WARNING);
    else
        gfx->setTextColor(COLOR_ERROR);

    gfx->print(rssi);
    gfx->println(" dBm");
    gfx->setTextColor(COLOR_TEXT);

    gfx->setCursor(10, 110);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->println("Uptime:");
    gfx->setTextColor(COLOR_TEXT);
    gfx->setCursor(20, 130);
    gfx->print(formatUptime());
    gfx->print("   ");
    gfx->println(startDateTimeString());

    drawPingRows();

    gfx->setTextColor(COLOR_TEXT);
    setDefaultFont();
    gfx->setTextSize(2);

    drawNav1("Back");
    drawNav2("Refresh");
    drawNav4("Home");
}

void enterWifiStatusScreen()
{
    state.currentScreen = SCREEN_WIFI_STATUS;
    drawCurrentScreen();
}

void handleWifiStatusTouch(int x, int y)
{
    if (navPressed1(x, y))
    {
        enterSystemScreen();
    }
    // Refresh button
    else if (navPressed2(x, y))
    {
        startPings();
        refreshWifiStatusResults();
    }

}
