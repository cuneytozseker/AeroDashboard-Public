#ifndef RADAR_PROCESSOR_H
#define RADAR_PROCESSOR_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <SharedConfig.h>

/**
 * RadarProcessor - Processes weather radar JPEG images
 *
 * Handles JPEG decoding, cropping, bilinear scaling, rain color extraction,
 * and Bayer dithering to produce 1-bit bitmap for e-paper display.
 */
class RadarProcessor {
public:
    RadarProcessor();
    ~RadarProcessor();

    /**
     * Process a radar JPEG image
     * @param jpegPath Path to JPEG file in SPIFFS
     * @param cropX X coordinate to start crop
     * @param cropY Y coordinate to start crop
     * @param cropSize Size of crop region (square)
     * @param outputSize Target output size (square)
     * @param avgDBZ Output: average dBZ value detected
     * @return Pointer to 1-bit bitmap (caller owns memory), or nullptr on failure
     */
    uint8_t* processRadarImage(const char* jpegPath,
                                int cropX, int cropY, int cropSize,
                                int outputSize,
                                float* avgDBZ = nullptr);

    // Get last processing error
    const String& getLastError() const { return _lastError; }

private:
    // JPEG decode callback (static for TJpgDec)
    static bool jpegOutputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);

    // Image processing stages
    bool decodeAndCrop(const char* jpegPath, int cropX, int cropY, int cropSize);
    bool scaleBilinear(int targetSize);
    bool extractRainColors();
    bool applyDithering(int size, float* avgDBZ);

    // Rain color detection
    uint8_t getDBZIntensity(uint16_t rgb565);

    // Buffers
    uint16_t* _cropBuffer;      // Cropped RGB565 data
    uint16_t* _scaledBuffer;    // Scaled RGB565 data
    uint8_t* _outputBitmap;     // Final 1-bit output

    // Dimensions
    int _cropSize;
    int _scaledSize;

    // Static reference for callback
    static RadarProcessor* _instance;
    static int _targetCropX, _targetCropY, _targetCropSize;

    String _lastError;
};

#endif // RADAR_PROCESSOR_H
