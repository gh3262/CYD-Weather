#include "app.h"
#include "adafruitio/adafruitio_manager.h"
#include <esp_heap_caps.h>

static const int SYSTEM_BUTTON_X = 220;
static const int SYSTEM_BUTTON_W = 92;
static const int SYSTEM_BUTTON_H = 36;
static const int SYSTEM_WIFI_Y = 49;
static const int SYSTEM_REFRESH_Y = SYSTEM_WIFI_Y + SYSTEM_BUTTON_H + 6;

static void drawBeepsButton()
{
    if (buttonBeepsEnabled())
        drawNav3("Beeps");
    else
        drawButton(NAV_X3, NAV_Y, NAV_WIDTH, NAV_HEIGHT, "Silent", COLOR_BUTTON_OPT);
}

void drawSystemScreen()
{
    clearScreen(COLOR_BACKGROUND);

    titleBar("SYSTEM");

    drawButton(SYSTEM_BUTTON_X, SYSTEM_WIFI_Y, SYSTEM_BUTTON_W, SYSTEM_BUTTON_H, "WiFi");
    drawButton(SYSTEM_BUTTON_X, SYSTEM_REFRESH_Y, SYSTEM_BUTTON_W, SYSTEM_BUTTON_H, "Refresh");

    setBodyFont();
    gfx->setTextSize(1);

    gfx->setCursor(10, 49);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("Free Heap: ");
    gfx->setTextColor(COLOR_TEXT);
    gfx->print(ESP.getFreeHeap() / 1024);
    gfx->println(" KB");

    gfx->setCursor(10, 69);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("Sketch Size: ");
    gfx->setTextColor(COLOR_TEXT);
    gfx->print(ESP.getSketchSize() / 1024);
    gfx->println(" KB");

    gfx->setCursor(10, 89);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("Min Heap: ");
    gfx->setTextColor(COLOR_TEXT);
    gfx->print(ESP.getMinFreeHeap() / 1024);
    gfx->println(" KB");

    gfx->setCursor(10, 109);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("Largest Block: ");
    gfx->setTextColor(COLOR_TEXT);
    gfx->print(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024);
    gfx->println(" KB");

    gfx->setCursor(10, 129);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("AIO Stack Free: ");
    gfx->setTextColor(COLOR_TEXT);
    size_t stackBytes;
    if (adafruitIOManager.getWorkerStackHeadroom(stackBytes))
    {
        gfx->print(stackBytes);
        gfx->println(" B");
    }
    else
        gfx->println("Not running");

    gfx->setCursor(10, 149);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("Uptime: ");
    gfx->setTextColor(COLOR_TEXT);
    gfx->println(formatUptime());
    gfx->setCursor(10, 169);
    gfx->println(startDateTimeString());

    gfx->setCursor(10, 189);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("MAC: ");
    gfx->setTextColor(COLOR_TEXT);
    gfx->print(WiFi.macAddress());

    drawNav1("Back");
    drawNav2("Tools");
    drawBeepsButton();
    drawNav4("Home");
}

void enterSystemScreen()
{
    state.currentScreen = SCREEN_SYSTEM;
    drawCurrentScreen();
}

void handleSystemTouch(int x, int y)
{
    if (buttonPressed(x, y, SYSTEM_BUTTON_X, SYSTEM_WIFI_Y, SYSTEM_BUTTON_W, SYSTEM_BUTTON_H))
    {
        enterWifiStatusScreen();
    }
    else if (buttonPressed(x, y, SYSTEM_BUTTON_X, SYSTEM_REFRESH_Y, SYSTEM_BUTTON_W, SYSTEM_BUTTON_H))
    {
        drawCurrentScreen();
    }
    // Back button
    else if (navPressed1(x, y))
    {
        enterHomeScreen();
    }

    // Tools button
    else if (navPressed2(x, y))
    {
        enterToolsScreen();
    }

    // Beeps/Silent toggle
    else if (navPressed3(x, y))
    {
        setButtonBeepsEnabled(!buttonBeepsEnabled());
        drawBeepsButton();
    }
}
