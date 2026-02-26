#include "OTAManager.h"

// Static member initialization
bool OTAManager::_isUpdating = false;

// =============================================================================
// Public Methods
// =============================================================================

void OTAManager::begin(const char* hostname, const char* password, uint16_t port) {
    // Set hostname for mDNS
    ArduinoOTA.setHostname(hostname);

    // Set authentication password
    if (password && strlen(password) > 0) {
        ArduinoOTA.setPassword(password);
    }

    // Set port
    ArduinoOTA.setPort(port);

    // Register callbacks
    ArduinoOTA.onStart(onStart);
    ArduinoOTA.onEnd(onEnd);
    ArduinoOTA.onProgress(onProgress);
    ArduinoOTA.onError(onError);

    // Start OTA service
    ArduinoOTA.begin();

    Serial.println("[OTA] Service started");
    Serial.printf("[OTA] Hostname: %s.local\n", hostname);
    Serial.printf("[OTA] Port: %d\n", port);
    if (password && strlen(password) > 0) {
        Serial.println("[OTA] Authentication: ENABLED");
    }
}

void OTAManager::handle() {
    ArduinoOTA.handle();
}

// =============================================================================
// Private Callbacks
// =============================================================================

void OTAManager::onStart() {
    _isUpdating = true;

    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
    } else {
        type = "filesystem";
    }

    Serial.println("\n========================================");
    Serial.printf("[OTA] Update started: %s\n", type.c_str());
    Serial.println("========================================");
}

void OTAManager::onEnd() {
    _isUpdating = false;

    Serial.println("\n========================================");
    Serial.println("[OTA] Update complete!");
    Serial.println("========================================");
}

void OTAManager::onProgress(unsigned int progress, unsigned int total) {
    static unsigned int lastPercent = 0;
    unsigned int percent = (progress * 100) / total;

    // Only print on percentage change to reduce spam
    if (percent != lastPercent && percent % 10 == 0) {
        Serial.printf("[OTA] Progress: %u%%\n", percent);
        lastPercent = percent;
    }
}

void OTAManager::onError(ota_error_t error) {
    _isUpdating = false;

    Serial.println("\n========================================");
    Serial.printf("[OTA] ERROR: ");

    switch (error) {
        case OTA_AUTH_ERROR:
            Serial.println("Authentication Failed");
            break;
        case OTA_BEGIN_ERROR:
            Serial.println("Begin Failed");
            break;
        case OTA_CONNECT_ERROR:
            Serial.println("Connect Failed");
            break;
        case OTA_RECEIVE_ERROR:
            Serial.println("Receive Failed");
            break;
        case OTA_END_ERROR:
            Serial.println("End Failed");
            break;
        default:
            Serial.println("Unknown Error");
            break;
    }

    Serial.println("========================================");
}
