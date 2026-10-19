#include "text_table_page.h"

#include "app.h"

static const size_t TEXT_TABLE_MAX_ROWS = 8;
static const int TEXT_TABLE_GUTTER_WIDTH = 20;
static const int TEXT_TABLE_FIRST_BASELINE = 50;
static const int TEXT_TABLE_ROW_SPACING = 19;
static const int TEXT_TABLE_ROW_ASCENT = 14;

struct RenderedTableRow
{
    String label;
    String value;
    TextTableColor labelColor;
    TextTableColor valueColor;
};

static RenderedTableRow renderedRows[TEXT_TABLE_MAX_ROWS];
static bool tableCacheValid = false;
static size_t renderedRowCount = 0;
static int renderedValueColumnX = 0;
static uint16_t renderedBackgroundColor = 0;

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

    const bool reset = !tableCacheValid ||
        renderedRowCount != page.rowCount ||
        renderedValueColumnX != page.valueColumnX ||
        renderedBackgroundColor != page.backgroundColor;
    if (reset)
    {
        gfx->fillRect(
            0, HEADER_HEIGHT, SCREEN_WIDTH, NAV_Y - HEADER_HEIGHT,
            page.backgroundColor);
    }
    setBodyFont();
    gfx->setTextSize(1);
    for (size_t i = 0; i < page.rowCount; ++i)
    {
        const TextTableRow &row = page.rows[i];
        const char *label = row.label ? row.label : "";
        const char *value = row.value ? row.value : "";
        RenderedTableRow &previous = renderedRows[i];
        const bool labelChanged = reset || previous.label != label ||
            previous.labelColor != row.labelColor;
        const bool valueChanged = reset || previous.value != value ||
            previous.valueColor != row.valueColor;
        if (!labelChanged && !valueChanged)
            continue;

        const int baseline = TEXT_TABLE_FIRST_BASELINE +
                             static_cast<int>(i) * TEXT_TABLE_ROW_SPACING;

        if (!reset)
        {
            const int clearX = labelChanged ? 0 : page.valueColumnX;
            gfx->fillRect(
                clearX, baseline - TEXT_TABLE_ROW_ASCENT,
                SCREEN_WIDTH - clearX, TEXT_TABLE_ROW_SPACING,
                page.backgroundColor);
        }

        if (labelChanged)
        {
            int16_t x1, y1;
            uint16_t width, height;
            gfx->getTextBounds(label, 0, 0, &x1, &y1, &width, &height);
            gfx->setTextColor(textTableColorValue(row.labelColor));
            const int labelRight = page.valueColumnX - TEXT_TABLE_GUTTER_WIDTH;
            gfx->setCursor(labelRight - (x1 + width) + 1, baseline);
            gfx->print(label);
        }

        // A label change clears the whole row, including its value.
        gfx->setTextColor(textTableColorValue(row.valueColor));
        gfx->setCursor(page.valueColumnX, baseline);
        gfx->print(value);

        previous.label = label;
        previous.value = value;
        previous.labelColor = row.labelColor;
        previous.valueColor = row.valueColor;
    }
    renderedRowCount = page.rowCount;
    renderedValueColumnX = page.valueColumnX;
    renderedBackgroundColor = page.backgroundColor;
    tableCacheValid = true;

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
    tableCacheValid = false;
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
