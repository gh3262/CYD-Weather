#include "text_table_page.h"

#include "app.h"

static const size_t TEXT_TABLE_MAX_ROWS = 8;
static const int TEXT_TABLE_GUTTER_WIDTH = 20;
static const int TEXT_TABLE_FIRST_BASELINE = 50;
static const int TEXT_TABLE_ROW_SPACING = 19;

uint16_t textTableColorValue(TextTableColor color)
{
    switch (color)
    {
    case TextTableColor::White:
        return CYD_WHITE;
    case TextTableColor::Yellow:
        return CYD_YELLOW;
    case TextTableColor::Cyan:
        return CYD_CYAN;
    case TextTableColor::Pink:
        return CYD_PINK;
    }
    return CYD_WHITE;
}

static bool isValidButton(const TextTableButton &button)
{
    return (button.label == nullptr) == (button.action == nullptr);
}

static bool isValidPage(const TextTablePage &page)
{
    return page.title &&
           page.backAction &&
           page.rowCount <= TEXT_TABLE_MAX_ROWS &&
           page.valueColumnX > TEXT_TABLE_GUTTER_WIDTH &&
           page.valueColumnX < SCREEN_WIDTH &&
           (page.rowCount == 0 || page.rows) &&
           isValidButton(page.button2) &&
           isValidButton(page.button3);
}

bool refreshTextTablePageRows(const TextTablePage &page)
{
    if (page.valueColumnX <= TEXT_TABLE_GUTTER_WIDTH ||
        page.valueColumnX >= SCREEN_WIDTH ||
        page.rowCount > TEXT_TABLE_MAX_ROWS ||
        (page.rowCount > 0 && !page.rows))
    {
        Serial.println("Text table: invalid row definition.");
        return false;
    }

    gfx->fillRect(
        0, HEADER_HEIGHT, SCREEN_WIDTH, NAV_Y - HEADER_HEIGHT,
        page.backgroundColor);
    setBodyFont();
    gfx->setTextSize(1);
    for (size_t i = 0; i < page.rowCount; ++i)
    {
        const TextTableRow &row = page.rows[i];
        const char *label = row.label ? row.label : "";
        const char *value = row.value ? row.value : "";
        const int baseline = TEXT_TABLE_FIRST_BASELINE +
                             static_cast<int>(i) * TEXT_TABLE_ROW_SPACING;

        int16_t x1, y1;
        uint16_t width, height;
        gfx->getTextBounds(label, 0, 0, &x1, &y1, &width, &height);
        gfx->setTextColor(textTableColorValue(row.labelColor));
        const int labelRight = page.valueColumnX - TEXT_TABLE_GUTTER_WIDTH;
        gfx->setCursor(labelRight - (x1 + width) + 1, baseline);
        gfx->print(label);

        gfx->setTextColor(textTableColorValue(row.valueColor));
        gfx->setCursor(page.valueColumnX, baseline);
        gfx->print(value);
    }

    gfx->setTextColor(COLOR_TEXT);
    setDefaultFont();
    gfx->setTextSize(2);
    return true;
}

bool drawTextTablePage(const TextTablePage &page)
{
    if (!isValidPage(page))
    {
        Serial.println("Text table: invalid page definition.");
        return false;
    }

    clearScreen(page.backgroundColor);
    titleBar(page.title);

    if (!refreshTextTablePageRows(page))
        return false;

    drawNav1("Back");
    if (page.button2.label)
        drawNav2(page.button2.label);
    if (page.button3.label)
        drawNav3(page.button3.label);
    drawNav4("Home");
    return true;
}

bool handleTextTablePageTouch(const TextTablePage &page, int x, int y)
{
    if (!page.backAction || !isValidButton(page.button2) ||
        !isValidButton(page.button3))
    {
        Serial.println("Text table: invalid page button definition.");
        return false;
    }

    if (navPressed1(x, y))
        page.backAction();
    else if (navPressed2(x, y) && page.button2.action)
        page.button2.action();
    else if (navPressed3(x, y) && page.button3.action)
        page.button3.action();
    else if (navPressed4(x, y))
        enterHomeScreen();
    else
        return false;

    return true;
}
