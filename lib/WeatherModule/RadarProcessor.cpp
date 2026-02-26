#include "RadarProcessor.h"
#include <TJpg_Decoder.h>

// Static members
RadarProcessor* RadarProcessor::_instance = nullptr;
int RadarProcessor::_targetCropX = 0;
int RadarProcessor::_targetCropY = 0;
int RadarProcessor::_targetCropSize = 0;

// Bayer 8x8 dithering matrix
const uint8_t BAYER_MATRIX[8][8] PROGMEM = {
    { 0, 32,  8, 40,  2, 34, 10, 42},
    {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44,  4, 36, 14, 46,  6, 38},
    {60, 28, 52, 20, 62, 30, 54, 22},
    { 3, 35, 11, 43,  1, 33,  9, 41},
    {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47,  7, 39, 13, 45,  5, 37},
    {63, 31, 55, 23, 61, 29, 53, 21}
};

// =============================================================================
// Constructor / Destructor
// =============================================================================

RadarProcessor::RadarProcessor()
    : _cropBuffer(nullptr)
    , _scaledBuffer(nullptr)
    , _outputBitmap(nullptr)
    , _cropSize(0)
    , _scaledSize(0)
{
}

RadarProcessor::~RadarProcessor() {
    if (_cropBuffer) free(_cropBuffer);
    if (_scaledBuffer) free(_scaledBuffer);
    // Note: _outputBitmap ownership is transferred to caller
}

// =============================================================================
// Main Processing Function
// =============================================================================

uint8_t* RadarProcessor::processRadarImage(const char* jpegPath,
                                            int cropX, int cropY, int cropSize,
                                            int outputSize,
                                            float* avgDBZ) {
    DEBUG_PRINTLN("[RadarProcessor] Starting radar processing...");

    // Step 1: Decode JPEG and crop region (with horizontal mirroring in callback)
    if (!decodeAndCrop(jpegPath, cropX, cropY, cropSize)) {
        return nullptr;
    }

    // Step 2: Scale to output size using bilinear interpolation
    if (!scaleBilinear(outputSize)) {
        return nullptr;
    }

    // Step 3: Extract rain colors (set non-rain to black)
    if (!extractRainColors()) {
        return nullptr;
    }

    // Step 4: Apply Bayer dithering to produce 1-bit output
    if (!applyDithering(outputSize, avgDBZ)) {
        return nullptr;
    }

    // Transfer ownership of output bitmap
    uint8_t* result = _outputBitmap;
    _outputBitmap = nullptr;

    // Clean up intermediate buffers
    if (_cropBuffer) {
        free(_cropBuffer);
        _cropBuffer = nullptr;
    }
    if (_scaledBuffer) {
        free(_scaledBuffer);
        _scaledBuffer = nullptr;
    }

    DEBUG_PRINTLN("[RadarProcessor] Processing complete");
    return result;
}

// =============================================================================
// JPEG Decoding
// =============================================================================

bool RadarProcessor::jpegOutputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (!_instance || !_instance->_cropBuffer) return false;

    // Check if this block overlaps with our crop region
    int cropEndX = _targetCropX + _targetCropSize;
    int cropEndY = _targetCropY + _targetCropSize;

    if (x + w <= _targetCropX || x >= cropEndX ||
        y + h <= _targetCropY || y >= cropEndY) {
        return true; // Skip blocks outside crop region
    }

    // Copy overlapping pixels to crop buffer (no mirroring needed for GxEPD2 drawBitmap)
    for (int by = 0; by < h; by++) {
        int srcY = y + by;
        if (srcY < _targetCropY || srcY >= cropEndY) continue;

        for (int bx = 0; bx < w; bx++) {
            int srcX = x + bx;
            if (srcX < _targetCropX || srcX >= cropEndX) continue;

            int dstX = srcX - _targetCropX;
            int dstY = srcY - _targetCropY;

            // Direct copy - no horizontal mirroring
            int dstIndex = dstY * _targetCropSize + dstX;

            _instance->_cropBuffer[dstIndex] = bitmap[by * w + bx];
        }
    }

    return true;
}

bool RadarProcessor::decodeAndCrop(const char* jpegPath, int cropX, int cropY, int cropSize) {
    DEBUG_PRINTF("[RadarProcessor] Decoding %s, crop: (%d,%d) %dx%d\n",
                jpegPath, cropX, cropY, cropSize, cropSize);

    // Ensure SPIFFS is mounted for TJpgDec to read files
    if (!SPIFFS.begin(true)) {
        _lastError = "SPIFFS mount failed";
        DEBUG_PRINTLN("[RadarProcessor] " + _lastError);
        return false;
    }

    // Allocate crop buffer in PSRAM if available
    _cropSize = cropSize;
    size_t bufferSize = cropSize * cropSize * sizeof(uint16_t);

    if (psramFound()) {
        _cropBuffer = (uint16_t*)ps_malloc(bufferSize);
    } else {
        _cropBuffer = (uint16_t*)malloc(bufferSize);
    }

    if (!_cropBuffer) {
        _lastError = "Failed to allocate crop buffer";
        DEBUG_PRINTLN("[RadarProcessor] " + _lastError);
        return false;
    }

    // Initialize buffer to black
    memset(_cropBuffer, 0, bufferSize);

    // Set up static callback parameters
    _instance = this;
    _targetCropX = cropX;
    _targetCropY = cropY;
    _targetCropSize = cropSize;

    // Configure TJpgDec
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(false);
    TJpgDec.setCallback(jpegOutputCallback);

    // Decode JPEG from SPIFFS
    // Note: TJpgDec library uses SPIFFS internally when using drawFsJpg
    uint16_t jpegW, jpegH;
    TJpgDec.getFsJpgSize(&jpegW, &jpegH, jpegPath);
    DEBUG_PRINTF("[RadarProcessor] JPEG size: %dx%d\n", jpegW, jpegH);

    // drawFsJpg returns JRESULT enum: JDR_OK (0) on success
    JRESULT result = TJpgDec.drawFsJpg(0, 0, jpegPath);
    if (result != JDR_OK) {
        DEBUG_PRINTF("[RadarProcessor] JPEG decode failed: %d\n", result);
        _lastError = "JPEG decode failed";
        return false;
    }

    // Verify crop buffer has data (check first few pixels)
    DEBUG_PRINTF("[RadarProcessor] Crop buffer sample pixels: [0]=%04X [100]=%04X [500]=%04X\n",
                 _cropBuffer[0], _cropBuffer[100], _cropBuffer[500]);

    return true;
}

// =============================================================================
// Bilinear Scaling
// =============================================================================

bool RadarProcessor::scaleBilinear(int targetSize) {
    DEBUG_PRINTF("[RadarProcessor] Scaling %d -> %d\n", _cropSize, targetSize);

    // Allocate scaled buffer
    _scaledSize = targetSize;
    size_t bufferSize = targetSize * targetSize * sizeof(uint16_t);

    if (psramFound()) {
        _scaledBuffer = (uint16_t*)ps_malloc(bufferSize);
    } else {
        _scaledBuffer = (uint16_t*)malloc(bufferSize);
    }

    if (!_scaledBuffer) {
        _lastError = "Failed to allocate scaled buffer";
        DEBUG_PRINTLN("[RadarProcessor] " + _lastError);
        return false;
    }

    // Initialize scaled buffer to black (prevent garbage data)
    memset(_scaledBuffer, 0, bufferSize);
    DEBUG_PRINTLN("[RadarProcessor] Scaled buffer initialized to zero");

    // Fixed-point scaling (8 fractional bits) - matching reference project
    const int FRAC_BITS = 8;
    const int FRAC_MULT = 1 << FRAC_BITS;

    // Use (size - 1) to account for pixel intervals (more accurate for bilinear)
    int scaleX = ((_cropSize - 1) * FRAC_MULT) / (targetSize - 1);
    int scaleY = ((_cropSize - 1) * FRAC_MULT) / (targetSize - 1);

    for (int dy = 0; dy < targetSize; dy++) {
        int srcY = (dy * scaleY) >> FRAC_BITS;
        int fracY = (dy * scaleY) & (FRAC_MULT - 1);

        for (int dx = 0; dx < targetSize; dx++) {
            int srcX = (dx * scaleX) >> FRAC_BITS;
            int fracX = (dx * scaleX) & (FRAC_MULT - 1);

            // Clamp to valid range
            int x0 = srcX;
            int y0 = srcY;
            int x1 = min(srcX + 1, _cropSize - 1);
            int y1 = min(srcY + 1, _cropSize - 1);

            // Get four neighboring pixels
            uint16_t p00 = _cropBuffer[y0 * _cropSize + x0];
            uint16_t p10 = _cropBuffer[y0 * _cropSize + x1];
            uint16_t p01 = _cropBuffer[y1 * _cropSize + x0];
            uint16_t p11 = _cropBuffer[y1 * _cropSize + x1];

            // Extract RGB components (RGB565)
            int r00 = (p00 >> 11) & 0x1F;
            int g00 = (p00 >> 5) & 0x3F;
            int b00 = p00 & 0x1F;

            int r10 = (p10 >> 11) & 0x1F;
            int g10 = (p10 >> 5) & 0x3F;
            int b10 = p10 & 0x1F;

            int r01 = (p01 >> 11) & 0x1F;
            int g01 = (p01 >> 5) & 0x3F;
            int b01 = p01 & 0x1F;

            int r11 = (p11 >> 11) & 0x1F;
            int g11 = (p11 >> 5) & 0x3F;
            int b11 = p11 & 0x1F;

            // Bilinear interpolation
            int invFracX = FRAC_MULT - fracX;
            int invFracY = FRAC_MULT - fracY;

            int r = (r00 * invFracX * invFracY +
                     r10 * fracX * invFracY +
                     r01 * invFracX * fracY +
                     r11 * fracX * fracY) >> (FRAC_BITS * 2);

            int g = (g00 * invFracX * invFracY +
                     g10 * fracX * invFracY +
                     g01 * invFracX * fracY +
                     g11 * fracX * fracY) >> (FRAC_BITS * 2);

            int b = (b00 * invFracX * invFracY +
                     b10 * fracX * invFracY +
                     b01 * invFracX * fracY +
                     b11 * fracX * fracY) >> (FRAC_BITS * 2);

            // Recombine to RGB565
            _scaledBuffer[dy * targetSize + dx] = ((r & 0x1F) << 11) |
                                                   ((g & 0x3F) << 5) |
                                                   (b & 0x1F);
        }
    }

    // Verify scaled buffer has data - sample multiple positions
    DEBUG_PRINTF("[RadarProcessor] Scaled buffer samples:\n");
    for (int i = 0; i < 10; i++) {
        int idx = i * (targetSize * targetSize / 10);
        uint16_t pixel = _scaledBuffer[idx];
        uint8_t r = ((pixel >> 11) & 0x1F) << 3;
        uint8_t g = ((pixel >> 5) & 0x3F) << 2;
        uint8_t b = (pixel & 0x1F) << 3;
        DEBUG_PRINTF("  [%d] RGB(%d,%d,%d)\n", idx, r, g, b);
    }

    return true;
}

// =============================================================================
// Rain Color Extraction
// =============================================================================

bool RadarProcessor::extractRainColors() {
    DEBUG_PRINTLN("[RadarProcessor] Extracting rain colors...");

    int rainPixels = 0;
    int totalPixels = _scaledSize * _scaledSize;

    for (int i = 0; i < totalPixels; i++) {
        uint16_t pixel = _scaledBuffer[i];

        // Extract RGB (expand to 8-bit)
        uint8_t r = ((pixel >> 11) & 0x1F) << 3;
        uint8_t g = ((pixel >> 5) & 0x3F) << 2;
        uint8_t b = (pixel & 0x1F) << 3;

        // First, explicitly reject ground/terrain colors
        // Ground appears as tan/beige/brown (balanced RGB or slightly warm)
        bool isGround = false;

        // Tan/Beige/Brown: R≈G≈B with slight red/yellow tint
        // If R, G, B are all within 60 of each other, it's likely ground
        int maxRGB = max(max(r, g), b);
        int minRGB = min(min(r, g), b);
        if ((maxRGB - minRGB) < 60) {
            isGround = true; // Grayish/beige - ground or water
        }

        // Yellow-ish brown (ground): R and G high, B lower, but not vivid colors
        if (r > 100 && g > 80 && b < 150 && abs((int)r - (int)g) < 50) {
            isGround = true; // Brownish ground
        }

        // Check for rain colors - adjusted for multiple radar sources
        bool isRain = false;

        if (!isGround) {
            // Light rain: Cyan/Blue - broader range for different radar sources
            // Accepts blue tints where B > G > R (blue dominant)
            if (b > g && g > r && b > 180) {
                isRain = true;
            }
            // Light rain: Cyan - relaxed threshold
            else if (r < 30 && g > 120 && b > 120 && abs((int)g - (int)b) < 50) {
                isRain = true;
            }
            // Light-Medium rain: Dark teal/green - much wider range
            else if (r < 50 && g > 80 && g < 160 && b > 70 && b < 150) {
                isRain = true;
            }
            // Medium rain: Green - more permissive
            else if (r < 50 && g > 150 && b < 110) {
                isRain = true;
            }
            // Heavy rain: Yellow/Red - more permissive with dominance check
            else if (r > 140 && r > g && r > b) {
                isRain = true;
            }
        }

        if (!isRain) {
            _scaledBuffer[i] = 0x0000; // Set to black (no rain)
        } else {
            rainPixels++;
        }
    }

    DEBUG_PRINTF("[RadarProcessor] Rain pixels: %d/%d (%.1f%%)\n",
                 rainPixels, totalPixels, (rainPixels * 100.0) / totalPixels);

    return true;
}

// =============================================================================
// Bayer Dithering
// =============================================================================

bool RadarProcessor::applyDithering(int size, float* avgDBZ) {
    DEBUG_PRINTLN("[RadarProcessor] Applying Bayer dithering...");

    // Allocate 1-bit output buffer
    int bytesPerRow = (size + 7) / 8;
    size_t bitmapSize = bytesPerRow * size;

    if (psramFound()) {
        _outputBitmap = (uint8_t*)ps_malloc(bitmapSize);
    } else {
        _outputBitmap = (uint8_t*)malloc(bitmapSize);
    }

    if (!_outputBitmap) {
        _lastError = "Failed to allocate output bitmap";
        DEBUG_PRINTLN("[RadarProcessor] " + _lastError);
        return false;
    }

    // Initialize to white (0 = white in most e-paper conventions)
    memset(_outputBitmap, 0x00, bitmapSize);

    float totalDBZ = 0;
    int rainPixels = 0;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            uint16_t pixel = _scaledBuffer[y * size + x];

            // Skip black pixels (non-rain areas set to 0x0000 by extractRainColors)
            if (pixel == 0x0000) {
                continue; // Leave as white (0x00) in output bitmap
            }

            // Get intensity from rain color
            uint8_t intensity = getDBZIntensity(pixel);

            if (intensity > 0) {
                totalDBZ += intensity;
                rainPixels++;
            }

            // Get Bayer threshold
            uint8_t threshold = pgm_read_byte(&BAYER_MATRIX[y % 8][x % 8]);

            // Scale threshold to 0-255 range (multiply by 4)
            bool isBlack = intensity > (threshold * 4);

            // Pack into bitmap (MSB first, 1 = black)
            if (isBlack) {
                int byteIndex = y * bytesPerRow + (x / 8);
                int bitIndex = 7 - (x % 8);
                _outputBitmap[byteIndex] |= (1 << bitIndex);
            }
        }
    }

    // Calculate average dBZ
    if (avgDBZ) {
        *avgDBZ = (rainPixels > 0) ? (totalDBZ / rainPixels) : 0;
    }

    DEBUG_PRINTF("[RadarProcessor] Dithering complete, rain pixels: %d\n", rainPixels);
    return true;
}

// =============================================================================
// dBZ Intensity Mapping
// =============================================================================

uint8_t RadarProcessor::getDBZIntensity(uint16_t rgb565) {
    // Black (no rain)
    if (rgb565 == 0x0000) return 0;

    // Extract RGB components (expand to 8-bit)
    uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;
    uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;
    uint8_t b = (rgb565 & 0x1F) << 3;

    // Map rain colors to dBZ intensity (0-255 scale)
    // Based on Turkish Meteorology radar color scheme - matching reference project

    // Cyan (light rain) -> Map to 30-100 range
    // Criteria: Low Red, High Green, High Blue, G≈B
    if (r < 50 && g > 140 && b > 140 && abs((int)g - (int)b) < 40) {
        int brightness = (g + b) / 2; // Range ~140-255
        // Map 140-255 -> 30-100
        return map(brightness, 140, 255, 30, 100);
    }

    // Teal (light-medium rain) -> Map to 100-150 range
    // Criteria: Low Red, Medium Green, Medium Blue
    if (r < 60 && g > 80 && g < 160 && b > 70 && b < 150) {
        // Use Green component as primary intensity driver
        return map(g, 80, 160, 100, 150);
    }

    // Green (medium rain) -> Map to 150-220 range
    // Criteria: Low Red, High Green, Low Blue
    if (r < 60 && g > 160 && b < 120) {
        // Map Green 160-255 -> 150-220
        return map(g, 160, 255, 150, 220);
    }

    // Yellow/Red (heavy rain) -> Map to 220-255 (Bright White)
    // Criteria: High Red
    if (r > 150) {
        // Map Red 150-255 -> 220-255
        return map(r, 150, 255, 220, 255);
    }

    // No match - should have been filtered by extractRainColors
    // Return 0 to keep these pixels white (non-rain)
    return 0;
}
