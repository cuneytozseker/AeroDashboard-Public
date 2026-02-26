#ifndef OTAMANAGER_H
#define OTAMANAGER_H

/**
 * =============================================================================
 * OTAManager - Over-The-Air Update Handler
 * =============================================================================
 * Manages ArduinoOTA for wireless firmware updates.
 * Provides callbacks for update progress and error handling.
 * =============================================================================
 */

#include <Arduino.h>
#include <ArduinoOTA.h>

class OTAManager {
public:
    /**
     * Initialize OTA service
     * @param hostname Network hostname for mDNS
     * @param password Authentication password
     * @param port OTA port (default 3232)
     */
    void begin(const char* hostname, const char* password, uint16_t port = 3232);

    /**
     * Handle OTA update requests (call frequently in loop)
     */
    void handle();

    /**
     * Check if OTA update is in progress
     */
    bool isUpdating() const { return _isUpdating; }

private:
    static bool _isUpdating;

    // ArduinoOTA callbacks
    static void onStart();
    static void onEnd();
    static void onProgress(unsigned int progress, unsigned int total);
    static void onError(ota_error_t error);
};

#endif // OTAMANAGER_H
