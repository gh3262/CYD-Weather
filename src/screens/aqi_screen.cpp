#include "app.h"

#include "adafruitio/adafruitio_manager.h"
#include "screens/aqi_page_values.h"

static const char *const AQI_FEED_KEYS[] = {"pages.p06-aqi"};
static const int AQI_MARGIN = 12;
static const int AQI_GAP = 8;
static const int AQI_COLUMN_WIDTH = (SCREEN_WIDTH - 2 * AQI_MARGIN - 2 * AQI_GAP) / 3;
static const int AQI_POLLUTANTS_Y = HEADER_HEIGHT + 86;
static const int AQI_TIMESTAMP_Y = NAV_Y - 8;
static String lastAqiPayload;
static bool forceAqiRender = true;
static bool hadAqiValue = false;

static void drawAqiText(const String &text, int x, int y, int width,
                        int size, uint16_t color, int maxLines = 1)
{
    String remaining = text;
    const int capacity = width / (6 * size);
    gfx->setTextSize(size);
    gfx->setTextColor(color);
    for (int line = 0; line < maxLines && !remaining.isEmpty(); ++line)
    {
        int length = remaining.length();
        if (length > capacity)
        {
            length = capacity;
            if (line + 1 < maxLines)
                for (int i = capacity; i > 0; --i)
                    if (remaining.c_str()[i] == ' ')
                    {
                        length = i;
                        break;
                    }
        }
        String part = remaining.substring(0, length);
        if (line + 1 == maxLines && remaining.length() > static_cast<unsigned>(length))
            part = part.substring(0, capacity - 3) + "...";
        gfx->setCursor(x + (width - static_cast<int>(part.length()) * 6 * size) / 2,
                       y + line * 8 * size);
        gfx->print(part);
        remaining = remaining.substring(length);
        remaining.trim();
    }
}

void refreshAqiResults()
{
    String payload;
    const bool available = adafruitIOManager.getFeedValue(AQI_FEED_KEYS[0], payload);
    if (!forceAqiRender && payload == lastAqiPayload && available == hadAqiValue)
        return;

    JsonDocument document;
    const char *error = nullptr;
    if (available)
    {
        const DeserializationError jsonError = deserializeJson(document, payload.c_str());
        error = jsonError ? jsonError.c_str() : validateAqiPageValues(document);
        if (error)
            Serial.printf("AQI: invalid %s: %s.\n", AQI_FEED_KEYS[0], error);
    }
    lastAqiPayload = payload;
    hadAqiValue = available;
    forceAqiRender = false;

    gfx->fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, NAV_Y - HEADER_HEIGHT, COLOR_BACKGROUND);
    setDefaultFont();
    gfx->setTextWrap(false);
    const char *const labels[] = {"Now", "Today", "Tomorrow"};
    const size_t offsets[] = {0, 4, 7};
    for (size_t i = 0; i < 3; ++i)
    {
        const int x = AQI_MARGIN + i * (AQI_COLUMN_WIDTH + AQI_GAP);
        uint16_t color = CYD_DARK_GRAY;
        uint32_t rgb = 0;
        const bool valid = available && !error;
        if (valid)
        {
            rgb = std::strtoul(document[offsets[i] + 2].as<const char *>() + 1, nullptr, 16);
            color = ((rgb >> 8) & 0xf800) | ((rgb >> 5) & 0x07e0) | ((rgb >> 3) & 0x001f);
        }
        const unsigned luminance = 299 * ((rgb >> 16) & 255) +
            587 * ((rgb >> 8) & 255) + 114 * (rgb & 255);
        gfx->fillRoundRect(x, HEADER_HEIGHT + 8, AQI_COLUMN_WIDTH, 22, 6, color);
        drawAqiText(labels[i], x, HEADER_HEIGHT + 15, AQI_COLUMN_WIDTH, 1,
                    valid && luminance >= 128000 ? 0x0000 : COLOR_TEXT);
        drawAqiText(valid ? document[offsets[i]].as<const char *>() : "--",
                    x, HEADER_HEIGHT + 35, AQI_COLUMN_WIDTH, 2, COLOR_TEXT);
        drawAqiText(valid ? document[offsets[i] + 1].as<const char *>() : "--",
                    x, HEADER_HEIGHT + 55, AQI_COLUMN_WIDTH, 1, COLOR_TEXT, 3);
    }
    if (available && !error)
    {
        gfx->setTextSize(1);
        gfx->setTextColor(COLOR_TEXT);
        for (size_t i = 10; i < document.size(); ++i)
        {
            const int y = AQI_POLLUTANTS_Y + (i - 10) * 11;
            if (y + 8 > AQI_TIMESTAMP_Y)
                break;
            String text = document[i].as<const char *>();
            const int capacity = (SCREEN_WIDTH - 2 * AQI_MARGIN) / 6;
            if (text.length() > static_cast<unsigned>(capacity))
                text = text.substring(0, capacity - 3) + "...";
            gfx->setCursor(AQI_MARGIN, y);
            gfx->print(text);
        }
        char timestamp[17];
        if (formatAqiTimestamp(document[3].as<const char *>(), timestamp))
            drawAqiText(timestamp, AQI_MARGIN, AQI_TIMESTAMP_Y,
                        SCREEN_WIDTH - 2 * AQI_MARGIN, 1, COLOR_TEXT);
    }
    else
    {
        drawAqiText(error ? "Invalid AQI data" : "Waiting for AQI data",
                    AQI_MARGIN, AQI_POLLUTANTS_Y, SCREEN_WIDTH - 2 * AQI_MARGIN,
                    1, COLOR_TEXT);
    }
    gfx->setTextWrap(true);
    gfx->setTextColor(COLOR_TEXT);
    gfx->setTextSize(2);
}

void drawAqiScreen()
{
    if (!adafruitIOManager.setActiveFeeds(AQI_FEED_KEYS, 1))
        Serial.println("AQI: could not activate pages.p06-aqi.");
    clearScreen(COLOR_BACKGROUND);
    titleBar("AQI");
    forceAqiRender = true;
    refreshAqiResults();
    drawNav1("Back");
    drawNav4("Home");
}

void enterAqiScreen()
{
    state.currentScreen = SCREEN_AQI;
    drawCurrentScreen();
}

void handleAqiTouch(int x, int y)
{
    if (navPressed1(x, y))
        enterDetailsScreen();
}
