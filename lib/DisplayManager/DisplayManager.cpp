#include "DisplayManager.h"
#include <SPIFFS.h>

DisplayManager::DisplayManager()
    : _display(GxEPD2_579_GDEY0579T93(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY))
    , _initialized(false)
    , _fullRefreshMode(true)
    , _inRenderCycle(false)
    , _windowX(0), _windowY(0), _windowW(DISPLAY_WIDTH), _windowH(DISPLAY_HEIGHT)
    , _shadowBuffer(nullptr)
    , _shadowBufferSize(0)
{
    // Allocate shadow buffer (1 bit per pixel)
    _shadowBufferSize = ((DISPLAY_WIDTH + 7) / 8) * DISPLAY_HEIGHT;
    _shadowBuffer = (uint8_t*)malloc(_shadowBufferSize);
    if (_shadowBuffer) {
        memset(_shadowBuffer, 0xFF, _shadowBufferSize); // Initialize to white
    }
}

DisplayManager::~DisplayManager() {
    if (_shadowBuffer) {
        free(_shadowBuffer);
        _shadowBuffer = nullptr;
    }
}

bool DisplayManager::begin() {
    pinMode(EPD_POWER_PIN, OUTPUT);
    digitalWrite(EPD_POWER_PIN, EPD_POWER_ON);
    delay(100);
    SPI.begin(EPD_SCK, -1, EPD_MOSI, -1);
    _display.init(115200, true, 2, false);
    if (psramFound()) {
        _display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    }
    _display.setRotation(0);
    _display.setFont(&FreeSans9pt7b);
    _display.setTextColor(BLACK);
    _initialized = true;
    return true;
}

void DisplayManager::setFullRefreshMode() { _fullRefreshMode = true; _display.setFullWindow(); }
void DisplayManager::setPartialRefreshMode() { _fullRefreshMode = false; _display.setPartialWindow(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT); }
void DisplayManager::beginRender() { _inRenderCycle = true; _display.firstPage(); }
void DisplayManager::endRender() {
#if ENABLE_DEBUG_SERVER
    // Copy GxEPD2's internal buffer to shadow buffer using the public getter
    const uint8_t* displayBuffer = _display.getBufferPtr();
    if (_shadowBuffer && displayBuffer) {
        DEBUG_PRINTLN("[DEBUG] Copying GxEPD2 buffer to shadow buffer...");
        unsigned long startTime = millis();

        memcpy(_shadowBuffer, displayBuffer, _shadowBufferSize);

        unsigned long elapsed = millis() - startTime;
        DEBUG_PRINTF("[DEBUG] Buffer copied in %lu ms\n", elapsed);
    }
#endif
    _display.nextPage();
    _inRenderCycle = false;
}

void DisplayManager::hibernate() {
    _display.hibernate();
    DEBUG_PRINTLN("[INFO] Display hibernated");
}
void DisplayManager::clear() {
    _display.fillScreen(WHITE);
}

void DisplayManager::fillScreen(uint16_t color) {
    _display.fillScreen(color);
}
void DisplayManager::drawPixel(int16_t x, int16_t y, uint16_t color) {
    _display.drawPixel(x, y, color);
}

void DisplayManager::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    _display.drawLine(x0, y0, x1, y1, color);
}

void DisplayManager::drawVerticalLine(int16_t x, int16_t y, int16_t h) {
    _display.drawFastVLine(x, y, h, BLACK);
}

void DisplayManager::drawHorizontalLine(int16_t x, int16_t y, int16_t w) {
    _display.drawFastHLine(x, y, w, BLACK);
}

void DisplayManager::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    _display.drawRect(x, y, w, h, color);
}

void DisplayManager::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    _display.fillRect(x, y, w, h, color);
}
void DisplayManager::drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    _display.drawCircle(x, y, r, color);
}

void DisplayManager::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    _display.fillCircle(x, y, r, color);
}
void DisplayManager::setFont(const GFXfont* font) { _display.setFont(font); }
void DisplayManager::setTextColor(uint16_t color) { _display.setTextColor(color); }
void DisplayManager::setTextSize(uint8_t size) { _display.setTextSize(size); }
void DisplayManager::setCursor(int16_t x, int16_t y) { _display.setCursor(x, y); }
void DisplayManager::print(const char* text) {
    _display.print(text);
#if ENABLE_DEBUG_SERVER
    // Text goes directly to GxEPD2 buffer, capture not possible without reading back
#endif
}

void DisplayManager::print(const String& text) { _display.print(text); }
void DisplayManager::print(int value) { _display.print(value); }
void DisplayManager::print(float value, int decimals) { _display.print(value, decimals); }
void DisplayManager::println(const char* text) { _display.println(text); }
void DisplayManager::println(const String& text) { _display.println(text); }

void DisplayManager::drawTTF(int16_t x, int16_t y, const char* text, uint8_t size, uint16_t color, FontType type, int16_t kerning) {
    // NOTE: Text rendering goes directly to GxEPD2's internal buffer
    // and doesn't call our drawPixel hook, so text won't appear in shadow buffer
    // This is a known limitation for the debug screenshot feature
    _display.setTextColor(color);
    const GFXfont* font;
    if (type == FontType::Bold) {
        if (size >= 21) font = &FreeSansBold24pt7b;        // 21-30+ → 24pt
        else if (size >= 15) font = &FreeSansBold18pt7b;   // 15-20 → 18pt
        else if (size >= 11) font = &FreeSansBold12pt7b;   // 11-14 → 12pt
        else font = &FreeSansBold9pt7b;                    // 0-10 → 9pt
    } else {
        font = &FreeSans9pt7b;
    }
    _display.setFont(font);
    int16_t bx, by; uint16_t bw, bh;
    _display.getTextBounds(text, 0, 0, &bx, &by, &bw, &bh);
    _display.setCursor(x, y - by);
    if (kerning == 0) {
        _display.print(text);
    } else {
        size_t len = strlen(text);
        for (size_t i = 0; i < len; i++) {
            _display.print(text[i]);
            if (i < len - 1) {
                _display.setCursor(_display.getCursorX() + kerning, _display.getCursorY());
            }
        }
    }
}

void DisplayManager::drawTTF(int16_t x, int16_t y, const String& text, uint8_t size, uint16_t color, FontType type, int16_t kerning) {
    drawTTF(x, y, text.c_str(), size, color, type, kerning);
}

void DisplayManager::getTTFBounds(const char* text, uint8_t size, int16_t* x, int16_t* y, uint16_t* w, uint16_t* h, FontType type, int16_t kerning) {
    if (type == FontType::Bold) {
        if (size >= 21) _display.setFont(&FreeSansBold24pt7b);        // 21-30+ → 24pt
        else if (size >= 15) _display.setFont(&FreeSansBold18pt7b);   // 15-20 → 18pt
        else if (size >= 11) _display.setFont(&FreeSansBold12pt7b);   // 11-14 → 12pt
        else _display.setFont(&FreeSansBold9pt7b);                    // 0-10 → 9pt
    } else {
        _display.setFont(&FreeSans9pt7b);
    }
    _display.getTextBounds(text, 0, 0, x, y, w, h);
    if (kerning != 0) {
        size_t len = strlen(text);
        if (len > 1) *w += kerning * (len - 1);
    }
}

void DisplayManager::getTextBounds(const char* text, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
    _display.getTextBounds(text, x, y, x1, y1, w, h);
}

// Alias functions for drawText (same as drawTTF with baseline fix)
void DisplayManager::drawText(int16_t x, int16_t y, const char* text, uint8_t size, uint16_t color, FontType type, int16_t kerning) {
    drawTTF(x, y, text, size, color, type, kerning);
}

void DisplayManager::drawText(int16_t x, int16_t y, const String& text, uint8_t size, uint16_t color, FontType type, int16_t kerning) {
    drawTTF(x, y, text.c_str(), size, color, type, kerning);
}

// Overloaded getTextBounds that matches getTTFBounds signature
void DisplayManager::getTextBounds(const char* text, uint8_t size, int16_t* x, int16_t* y, uint16_t* w, uint16_t* h, FontType type, int16_t kerning) {
    getTTFBounds(text, size, x, y, w, h, type, kerning);
}

void DisplayManager::drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color) {
    _display.drawBitmap(x, y, bitmap, w, h, color);
}

void DisplayManager::drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t fgColor, uint16_t bgColor) {
    _display.drawBitmap(x, y, bitmap, w, h, fgColor, bgColor);
}

void DisplayManager::drawMonoBitmap(int16_t x, int16_t y, const uint8_t* buffer, int16_t w, int16_t h) {
    int bytesPerRow = (w + 7) / 8;
    for (int16_t row = 0; row < h; row++) {
        for (int16_t col = 0; col < w; col++) {
            int byteIndex = row * bytesPerRow + (col / 8);
            int bitIndex = 7 - (col % 8);
            bool isBlack = (buffer[byteIndex] >> bitIndex) & 0x01;
            _display.drawPixel(x + col, y + row, isBlack ? BLACK : WHITE);
        }
    }
}

void DisplayManager::drawBackground(const char* path) {
    if (!SPIFFS.exists(path)) return;
    File file = SPIFFS.open(path, "r");
    size_t size = file.size();
    uint8_t* buffer = (uint8_t*)(psramFound() ? ps_malloc(size) : malloc(size));
    if (buffer) {
        file.read(buffer, size);
        file.close();
        _display.drawBitmap(0, 0, buffer, DISPLAY_WIDTH, DISPLAY_HEIGHT, BLACK);
        free(buffer);
    } else {
        file.close();
    }
}

void DisplayManager::setPartialWindow(int16_t x, int16_t y, int16_t w, int16_t h) {
    _windowX = x; _windowY = y; _windowW = w; _windowH = h;
    _display.setPartialWindow(x, y, w, h);
}

void DisplayManager::resetWindow() {
    _windowX = 0; _windowY = 0; _windowW = DISPLAY_WIDTH; _windowH = DISPLAY_HEIGHT;
    _display.setFullWindow();
}

const uint8_t* DisplayManager::getBuffer() const {
    return _shadowBuffer;
}

void DisplayManager::updateShadowBuffer() {
    // Shadow buffer is updated via direct memcpy in endRender()
}