#include "app.h"

#include "adafruitio/adafruitio_manager.h"
#include "screens/text_table_page.h"
#include "widgets/thermometer_widget.h"

static const char *const DETAILS_FEED_KEYS[] = {
    "feed-avg",
    "currenttemperatureapparent",
};

static const int GRID_COLUMNS = 2;
static const int GRID_ROWS = 4;
static const int GRID_WIDTH = SCREEN_WIDTH * 2 / 3;
static const int GRID_GAP = 4;
static const int GRID_BUTTON_WIDTH =
    (GRID_WIDTH - (GRID_COLUMNS + 1) * GRID_GAP) / GRID_COLUMNS;
static const int GRID_BUTTON_HEIGHT =
    (NAV_Y - HEADER_HEIGHT - (GRID_ROWS + 1) * GRID_GAP) / GRID_ROWS;

static const TextTableButton DETAIL_BUTTONS[GRID_COLUMNS * GRID_ROWS] = {
    {"Temps", enterTemperaturesScreen},
    {"Wind", enterWindScreen},
    {"Baro", enterBarometerScreen},
    {nullptr, nullptr},
    {"Precip", enterPrecipitationScreen},
    {nullptr, nullptr},
    {"Astro", enterAstronomicalScreen},
    {nullptr, nullptr},
};

static String renderedTemperature;
static String renderedApparentTemperature;
static bool forceThermometerRender = true;

static int gridButtonX(int index)
{
    return GRID_GAP + (index % GRID_COLUMNS) * (GRID_BUTTON_WIDTH + GRID_GAP);
}

static int gridButtonY(int index)
{
    return HEADER_HEIGHT + GRID_GAP +
           (index / GRID_COLUMNS) * (GRID_BUTTON_HEIGHT + GRID_GAP);
}

void refreshDetailsResults()
{
    String temperature;
    String apparent;
    adafruitIOManager.getFeedValue("feed-avg", temperature);
    adafruitIOManager.getFeedValue("currenttemperatureapparent", apparent);
    temperature.trim();
    apparent.trim();
    if (!forceThermometerRender &&
        temperature == renderedTemperature && apparent == renderedApparentTemperature)
        return;

    ThermometerWidgetOptions options;
    options.x = GRID_WIDTH;
    options.width = SCREEN_WIDTH - GRID_WIDTH;
    options.minimumTemperature = 10.0f;
    options.maximumTemperature = 100.0f;
    options.showCelsiusScale = true;
    options.backgroundColor = COLOR_BACKGROUND;

    if (parseTemperatureFeedValue(temperature, options.temperature))
    {
        options.hasApparentTemperature =
            parseTemperatureFeedValue(apparent, options.apparentTemperature);
        if (!drawThermometerWidget(options))
            return;
    }
    else
    {
        // Do not display the widget's default temperature while data is missing.
        gfx->fillRect(options.x, options.y, options.width, options.height,
                      options.backgroundColor);
        drawButton(options.x + 4, HEADER_HEIGHT + 60, options.width - 8, 36,
                   "-- F", options.backgroundColor, COLOR_TEXT);
    }

    renderedTemperature = temperature;
    renderedApparentTemperature = apparent;
    forceThermometerRender = false;
}

void drawDetailsScreen()
{
    if (!adafruitIOManager.setActiveFeeds(
            DETAILS_FEED_KEYS, sizeof(DETAILS_FEED_KEYS) / sizeof(DETAILS_FEED_KEYS[0])))
    {
        Serial.println("Details: could not activate its Adafruit IO feeds.");
    }

    clearScreen(COLOR_BACKGROUND);
    titleBar("Details");
    for (int i = 0; i < GRID_COLUMNS * GRID_ROWS; ++i)
    {
        const TextTableButton &button = DETAIL_BUTTONS[i];
        drawButton(gridButtonX(i), gridButtonY(i),
                   GRID_BUTTON_WIDTH, GRID_BUTTON_HEIGHT,
                   button.label ? button.label : "",
                   button.action ? COLOR_BUTTON : COLOR_BUTTON_OPT);
    }

    forceThermometerRender = true;
    refreshDetailsResults();
    drawNav1("Back");
    drawNav4("Home");
}

void enterDetailsScreen()
{
    state.currentScreen = SCREEN_DETAILS;
    drawCurrentScreen();
}

void handleDetailsTouch(int x, int y)
{
    if (navPressed1(x, y))
    {
        enterHomeScreen();
        return;
    }

    for (int i = 0; i < GRID_COLUMNS * GRID_ROWS; ++i)
    {
        if (DETAIL_BUTTONS[i].action &&
            touchInRect(x, y, gridButtonX(i), gridButtonY(i),
                        GRID_BUTTON_WIDTH, GRID_BUTTON_HEIGHT))
        {
            DETAIL_BUTTONS[i].action();
            return;
        }
    }
}
