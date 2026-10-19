#include "text_block_page.h"

#include "app.h"
#include <vector>

static const int TEXT_BLOCK_LEFT = 8;
static const int TEXT_BLOCK_RIGHT = SCREEN_WIDTH - 8;
static const int TEXT_BLOCK_FIRST_BASELINE = 50;
static const int TEXT_BLOCK_LINE_SPACING = 19;

struct WrappedLine
{
    size_t offset;
    size_t length;
};

static String cachedSource;
static String cachedText;
static std::vector<WrappedLine> cachedLines;
static bool layoutCached = false;

static bool isValidButton(const TextTableButton &button)
{
    return (button.label == nullptr) == (button.action == nullptr);
}

static bool isValidPage(const TextBlockPage &page)
{
    return page.title &&
           page.backAction &&
           isValidButton(page.button2) &&
           isValidButton(page.button3);
}

// The GFX fonts only cover printable ASCII, so normalize feed text first.
static String sanitizeText(const char *text)
{
    String result;
    if (!text)
        return result;

    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p)
    {
        if (*p == '\n')
            result += '\n';
        else if (*p == '\t')
            result += ' ';
        else if (*p == 0xC2 && p[1] == 0xB0)
        {
            result += ' ';  // "56.9°F" becomes "56.9 F"
            ++p;
        }
        else if (*p >= 0x20 && *p < 0x7F)
            result += static_cast<char>(*p);
    }
    result.trim();
    return result;
}

static int textWidth(const String &text)
{
    if (text.isEmpty())
        return 0;

    int16_t x1, y1;
    uint16_t width, height;
    gfx->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &width, &height);
    return x1 + width;
}

class LineWriter
{
public:
    explicit LineWriter(int maxWidth) : _maxWidth(maxWidth) {}

    void emit(const String &line)
    {
        cachedLines.push_back({cachedText.length(), line.length()});
        cachedText += line;
    }

    // Splits a word that cannot fit on a line by itself.
    String emitOversizedWord(String word)
    {
        while (textWidth(word) > _maxWidth && word.length() > 1)
        {
            size_t fit = 1;
            while (fit < word.length() &&
                   textWidth(word.substring(0, fit + 1)) <= _maxWidth)
                ++fit;
            emit(word.substring(0, fit));
            word = word.substring(fit);
        }
        return word;
    }

    int maxWidth() const { return _maxWidth; }

private:
    int _maxWidth;
};

static bool layoutText(const String &text)
{
    LineWriter writer(TEXT_BLOCK_RIGHT - TEXT_BLOCK_LEFT);
    cachedText = "";
    cachedLines.clear();
    if (text.isEmpty())
        return true;

    if (!cachedText.reserve(text.length()))
    {
        Serial.println("Text block: could not allocate wrapped text cache.");
        return false;
    }

    int paragraphStart = 0;
    while (paragraphStart <= static_cast<int>(text.length()))
    {
        int paragraphEnd = text.indexOf('\n', paragraphStart);
        if (paragraphEnd < 0)
            paragraphEnd = text.length();

        const String paragraph = text.substring(paragraphStart, paragraphEnd);
        String line;
        int wordStart = 0;
        while (wordStart < static_cast<int>(paragraph.length()))
        {
            if (paragraph[wordStart] == ' ')
            {
                ++wordStart;
                continue;
            }

            int wordEnd = paragraph.indexOf(' ', wordStart);
            if (wordEnd < 0)
                wordEnd = paragraph.length();
            String word = paragraph.substring(wordStart, wordEnd);
            wordStart = wordEnd;

            const String candidate = line.isEmpty() ? word : line + " " + word;
            if (textWidth(candidate) <= writer.maxWidth())
            {
                line = candidate;
                continue;
            }

            if (!line.isEmpty())
                writer.emit(line);
            line = writer.emitOversizedWord(word);
        }

        // An empty paragraph keeps a blank line between paragraphs.
        writer.emit(line);
        paragraphStart = paragraphEnd + 1;
    }
    return true;
}

bool refreshTextBlockPageText(const TextBlockPage &page, size_t *lineCount)
{
    setBodyFont();
    gfx->setTextSize(1);
    const char *source = page.text ? page.text : "";
    if (!layoutCached || cachedSource != source)
    {
        layoutCached = false;
        if (!cachedSource.reserve(strlen(source)))
        {
            Serial.println("Text block: could not allocate source text cache.");
            setDefaultFont();
            gfx->setTextSize(2);
            return false;
        }
        cachedSource = source;
        if (!layoutText(sanitizeText(source)))
        {
            setDefaultFont();
            gfx->setTextSize(2);
            return false;
        }
        layoutCached = true;
    }

    gfx->fillRect(
        0, HEADER_HEIGHT, SCREEN_WIDTH, NAV_Y - HEADER_HEIGHT,
        page.backgroundColor);
    gfx->setTextWrap(false);
    gfx->setTextColor(textTableColorValue(page.textColor));

    const size_t lines = cachedLines.size();
    for (size_t row = 0;
         row < TEXT_BLOCK_VISIBLE_LINES &&
         page.firstLine < lines && row < lines - page.firstLine;
         ++row)
    {
        const WrappedLine &line = cachedLines[page.firstLine + row];
        gfx->setCursor(
            TEXT_BLOCK_LEFT,
            TEXT_BLOCK_FIRST_BASELINE + static_cast<int>(row) * TEXT_BLOCK_LINE_SPACING);
        for (size_t i = 0; i < line.length; ++i)
            gfx->write(static_cast<uint8_t>(cachedText[line.offset + i]));
    }
    if (lineCount)
        *lineCount = lines;

    // Small down-arrow below the last line when more text follows.
    if (lines > page.firstLine + TEXT_BLOCK_VISIBLE_LINES)
    {
        const int tipY = NAV_Y - 3;
        const int centerX = TEXT_BLOCK_RIGHT - 4;
        gfx->fillTriangle(centerX - 5, tipY - 6, centerX + 5, tipY - 6,
                          centerX, tipY, textTableColorValue(page.textColor));
    }

    gfx->setTextWrap(true);
    gfx->setTextColor(COLOR_TEXT);
    setDefaultFont();
    gfx->setTextSize(2);
    return true;
}

bool drawTextBlockPage(const TextBlockPage &page, size_t *lineCount)
{
    if (!isValidPage(page))
    {
        Serial.println("Text block: invalid page definition.");
        return false;
    }

    clearScreen(page.backgroundColor);
    titleBar(page.title);

    if (!refreshTextBlockPageText(page, lineCount))
        return false;

    drawNav1("Back");
    if (page.button2.label)
        drawNav2(page.button2.label);
    if (page.button3.label)
        drawNav3(page.button3.label);
    drawNav4("Home");
    return true;
}

bool handleTextBlockPageTouch(const TextBlockPage &page, int x, int y)
{
    if (!isValidPage(page))
    {
        Serial.println("Text block: invalid page button definition.");
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
    else if (page.textTapAction &&
             touchInRect(x, y, 0, HEADER_HEIGHT, SCREEN_WIDTH, NAV_Y - HEADER_HEIGHT - 1))
        page.textTapAction();
    else
        return false;

    return true;
}
