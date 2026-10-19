#ifndef TEXT_TABLE_PAGE_H
#define TEXT_TABLE_PAGE_H

#include <Arduino.h>

enum class TextTableColor
{
    White,
    Yellow,
    Cyan,
    Pink
};

struct TextTableRow
{
    const char *label;
    const char *value;
    TextTableColor labelColor;
    TextTableColor valueColor;
};

struct TextTableButton
{
    const char *label;
    void (*action)();
};

struct TextTablePage
{
    const char *title;
    uint16_t backgroundColor;
    int valueColumnX;
    const TextTableRow *rows;
    size_t rowCount;
    void (*backAction)();
    TextTableButton button2;
    TextTableButton button3;
};

uint16_t textTableColorValue(TextTableColor color);

bool drawTextTablePage(const TextTablePage &page);
// Tracks the currently drawn table and updates only changed values/rows.
// Use drawTextTablePage() on entry to reset the renderer's cache.
bool refreshTextTablePageRows(const TextTablePage &page);
bool handleTextTablePageTouch(const TextTablePage &page, int x, int y);

#endif
