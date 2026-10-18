#ifndef TEXT_BLOCK_PAGE_H
#define TEXT_BLOCK_PAGE_H

#include <Arduino.h>

#include "screens/text_table_page.h"

static const size_t TEXT_BLOCK_VISIBLE_LINES = 8;

struct TextBlockPage
{
    const char *title;
    uint16_t backgroundColor;
    const char *text;
    TextTableColor textColor;
    // First wrapped line to show; lets a page scroll through long text.
    size_t firstLine;
    void (*backAction)();
    TextTableButton button2;
    TextTableButton button3;
    // Optional action when the text area is tapped (e.g. page down).
    void (*textTapAction)();
};

// lineCount (optional) receives the total number of wrapped lines.
bool drawTextBlockPage(const TextBlockPage &page, size_t *lineCount = nullptr);
bool refreshTextBlockPageText(const TextBlockPage &page, size_t *lineCount = nullptr);
bool handleTextBlockPageTouch(const TextBlockPage &page, int x, int y);

#endif
