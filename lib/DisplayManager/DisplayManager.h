#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <SharedConfig.h>
#include <Pins.h>

// Font includes (Adafruit GFX fonts)
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

/**
 * DisplayManager - Wrapper for GxEPD2 e-paper display
 */
class DisplayManager {
public:
    using DisplayType = GxEPD2_BW<GxEPD2_579_GDEY0579T93, GxEPD2_579_GDEY0579T93::HEIGHT>;

    DisplayManager();
    ~DisplayManager();

    bool begin();
    void setFullRefreshMode();
    void setPartialRefreshMode();
    void beginRender();
    void endRender();
    void hibernate();
    void clear();
    void fillScreen(uint16_t color);

    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void drawVerticalLine(int16_t x, int16_t y, int16_t h);
    void drawHorizontalLine(int16_t x, int16_t y, int16_t w);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color);

    void setFont(const GFXfont* font);
    void setTextColor(uint16_t color);
    void setTextSize(uint8_t size);
    void setCursor(int16_t x, int16_t y);
    void print(const char* text);
    void print(const String& text);
    void print(int value);
    void print(float value, int decimals = 1);
    void println(const char* text);
    void println(const String& text);

    enum class FontType { Normal, Bold };
    void drawTTF(int16_t x, int16_t y, const char* text, uint8_t size, uint16_t color = BLACK, FontType type = FontType::Normal, int16_t kerning = 0);
    void drawTTF(int16_t x, int16_t y, const String& text, uint8_t size, uint16_t color = BLACK, FontType type = FontType::Normal, int16_t kerning = 0);
    void getTTFBounds(const char* text, uint8_t size, int16_t* x, int16_t* y, uint16_t* w, uint16_t* h, FontType type = FontType::Normal, int16_t kerning = 0);

    // Alias for drawTTF (with baseline positioning fix)
    void drawText(int16_t x, int16_t y, const char* text, uint8_t size, uint16_t color = BLACK, FontType type = FontType::Normal, int16_t kerning = 0);
    void drawText(int16_t x, int16_t y, const String& text, uint8_t size, uint16_t color = BLACK, FontType type = FontType::Normal, int16_t kerning = 0);

    // Original getTextBounds (underlying GFX function wrapper)
    void getTextBounds(const char* text, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h);
    // Overloaded getTextBounds to match getTTFBounds signature
    void getTextBounds(const char* text, uint8_t size, int16_t* x, int16_t* y, uint16_t* w, uint16_t* h, FontType type = FontType::Normal, int16_t kerning = 0);
    void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color);
    void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t fgColor, uint16_t bgColor);
    void drawMonoBitmap(int16_t x, int16_t y, const uint8_t* buffer, int16_t w, int16_t h);
    void drawBackground(const char* path);

    void setPartialWindow(int16_t x, int16_t y, int16_t w, int16_t h);
    void resetWindow();

    int16_t width() const { return DISPLAY_WIDTH; }
    int16_t height() const { return DISPLAY_HEIGHT; }
    DisplayType& getDisplay() { return _display; }

    // Get framebuffer for debug purposes
    const uint8_t* getBuffer() const;

    static const uint16_t BLACK = GxEPD_BLACK;
    static const uint16_t WHITE = GxEPD_WHITE;

private:
    DisplayType _display;
    bool _initialized;
    bool _fullRefreshMode;
    bool _inRenderCycle;
    int16_t _windowX, _windowY, _windowW, _windowH;

    // Shadow buffer for debug access (1 bit per pixel)
    // Only used when ENABLE_DEBUG_SERVER is true
    uint8_t* _shadowBuffer;
    size_t _shadowBufferSize;
    void updateShadowBuffer();

    // Friend class to allow buffer access
    friend class DebugServer;
};

#endif