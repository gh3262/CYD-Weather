#include "app.h"

void drawSystemScreen()
{
    clearScreen(COLOR_BACKGROUND);

    titleBar("SYSTEM");

    drawButton(220, 49, 92, 36, "WiFi");

    setBodyFont();
    gfx->setTextSize(1);

    gfx->setCursor(10, 59);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("Free Heap: ");
    gfx->setTextColor(COLOR_TEXT);
    gfx->print(ESP.getFreeHeap() / 1024);
    gfx->println(" KB");

    gfx->setCursor(10, 79);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("Sketch Size: ");
    gfx->setTextColor(COLOR_TEXT);
    gfx->print(ESP.getSketchSize() / 1024);
    gfx->println(" KB");

    gfx->setCursor(10, 99);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("Min Heap: ");
    gfx->setTextColor(COLOR_TEXT);
    gfx->print(ESP.getMinFreeHeap() / 1024);
    gfx->println(" KB");

    gfx->setCursor(10, 139);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->println("Uptime:");
    gfx->setTextColor(COLOR_TEXT);
    gfx->setCursor(20, 159);
    gfx->print(formatUptime());
    gfx->print("   ");
    gfx->println(startDateTimeString());

    gfx->setCursor(10, 189);
    gfx->setTextColor(COLOR_LABEL_TEXT);
    gfx->print("MAC: ");
    gfx->setTextColor(COLOR_TEXT);
    gfx->print(WiFi.macAddress());

    drawNav1("Back");
    drawNav2("Tools");
    drawNav3("Refresh");
    drawNav4("Home");
}

void enterSystemScreen()
{
    state.currentScreen = SCREEN_SYSTEM;
    drawCurrentScreen();
}

void handleSystemTouch(int x, int y)
{
    if (touchInRect(x, y, 220, 49, 92, 36))
    {
        enterWifiStatusScreen();
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

    // Refresh button
    else if (navPressed3(x, y))
    {
        drawCurrentScreen();
    }
}
