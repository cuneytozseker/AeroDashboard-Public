#include "NetworkManager.h"
#include <SPIFFS.h>

// =============================================================================
// Constructor
// =============================================================================

NetworkManager::NetworkManager()
    : _timeSynced(false)
    , _timeClient(nullptr)
{
}

// =============================================================================
// WiFi Management
// =============================================================================

void NetworkManager::setCredentials(const String& ssid, const String& password) {
    _ssid = ssid;
    _password = password;
}

bool NetworkManager::connect(unsigned long timeoutMs) {
    if (_ssid.length() == 0) {
        DEBUG_PRINTLN("[NetworkManager] No SSID configured");
        return false;
    }

    DEBUG_PRINTF("[NetworkManager] Connecting to %s...\n", _ssid.c_str());

    // Disconnect any existing connection
    WiFi.disconnect(true);
    delay(100);

    // Set WiFi mode and begin connection
    WiFi.mode(WIFI_STA);

    // Configure DNS servers (Google DNS primary, Cloudflare secondary)
    IPAddress dns1(8, 8, 8, 8);      // Google DNS
    IPAddress dns2(1, 1, 1, 1);      // Cloudflare DNS
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, dns1, dns2);

    WiFi.begin(_ssid.c_str(), _password.c_str());

    // Wait for connection with timeout
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > timeoutMs) {
            DEBUG_PRINTLN("[NetworkManager] Connection timeout");
            WiFi.disconnect();
            return false;
        }
        delay(500);
        DEBUG_PRINT(".");
    }

    DEBUG_PRINTLN();
    DEBUG_PRINTF("[NetworkManager] Connected! IP: %s, RSSI: %d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());

    return true;
}

void NetworkManager::disconnect() {
    DEBUG_PRINTLN("[NetworkManager] Disconnecting WiFi");

    if (_timeClient) {
        delete _timeClient;
        _timeClient = nullptr;
    }

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

bool NetworkManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

IPAddress NetworkManager::getIPAddress() const {
    return WiFi.localIP();
}

int8_t NetworkManager::getRSSI() const {
    return WiFi.RSSI();
}

String NetworkManager::getSSID() const {
    return WiFi.SSID();
}

// =============================================================================
// Time Synchronization
// =============================================================================

bool NetworkManager::syncTime(int timezoneOffsetHours) {
    if (!isConnected()) {
        DEBUG_PRINTLN("[NetworkManager] Cannot sync time - not connected");
        return false;
    }

    DEBUG_PRINTLN("[NetworkManager] Syncing time via NTP...");

    // Clean up existing client
    if (_timeClient) {
        delete _timeClient;
    }

    // Create NTP client with timezone offset (in seconds)
    _timeClient = new NTPClient(_ntpUDP, NTP_SERVER, timezoneOffsetHours * 3600, NTP_UPDATE_INTERVAL_MS);

    _timeClient->begin();

    // Try to update time (with retries)
    for (int i = 0; i < 3; i++) {
        if (_timeClient->update()) {
            _timeSynced = true;

            // Set system time
            time_t epochTime = _timeClient->getEpochTime();
            struct timeval tv = { .tv_sec = epochTime, .tv_usec = 0 };
            settimeofday(&tv, nullptr);

            DEBUG_PRINTF("[NetworkManager] Time synced: %s\n",
                        _timeClient->getFormattedTime().c_str());
            return true;
        }
        delay(1000);
    }

    DEBUG_PRINTLN("[NetworkManager] NTP sync failed");
    return false;
}

time_t NetworkManager::getEpochTime() const {
    if (_timeClient && _timeSynced) {
        return _timeClient->getEpochTime();
    }
    return time(nullptr);
}

String NetworkManager::getFormattedTime() const {
    if (_timeClient && _timeSynced) {
        return _timeClient->getFormattedTime();
    }

    // Fallback to system time
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char buffer[10];
    strftime(buffer, sizeof(buffer), "%H:%M", timeinfo);
    return String(buffer);
}

// =============================================================================
// HTTP Helpers
// =============================================================================

String NetworkManager::httpGet(const String& url, int* httpCode) {
    if (!isConnected()) {
        DEBUG_PRINTLN("[NetworkManager] HTTP GET failed - not connected");
        if (httpCode) *httpCode = -1;
        return "";
    }

    DEBUG_PRINTF("[NetworkManager] HTTP GET: %s\n", url.c_str());

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(url);

    int code = http.GET();

    if (httpCode) *httpCode = code;

    String payload = "";
    if (code == HTTP_CODE_OK) {
        payload = http.getString();
        DEBUG_PRINTF("[NetworkManager] Response: %d bytes\n", payload.length());
    } else {
        DEBUG_PRINTF("[NetworkManager] HTTP error: %d\n", code);
    }

    http.end();
    return payload;
}

String NetworkManager::httpsGet(const String& url, const String& authHeader,
                                 const char* rootCA, int* httpCode) {
    if (!isConnected()) {
        DEBUG_PRINTLN("[NetworkManager] HTTPS GET failed - not connected");
        if (httpCode) *httpCode = -1;
        return "";
    }

    DEBUG_PRINTF("[NetworkManager] HTTPS GET: %s\n", url.c_str());

    WiFiClientSecure client;

    // Set certificate if provided
    if (rootCA) {
        client.setCACert(rootCA);
    } else {
        // Skip certificate verification (not recommended for production)
        client.setInsecure();
    }

    client.setTimeout(HTTP_TIMEOUT_MS / 1000); // setTimeout expects seconds

    HTTPClient https;
    https.setTimeout(HTTP_TIMEOUT_MS);

    if (!https.begin(client, url)) {
        DEBUG_PRINTLN("[NetworkManager] HTTPS begin failed");
        if (httpCode) *httpCode = -1;
        return "";
    }

    // Add authorization header if provided
    if (authHeader.length() > 0) {
        https.addHeader("Authorization", authHeader);
    }
    https.addHeader("Content-Type", "application/json");

    int code = https.GET();

    if (httpCode) *httpCode = code;

    String payload = "";
    if (code == HTTP_CODE_OK) {
        payload = https.getString();
        DEBUG_PRINTF("[NetworkManager] Response: %d bytes\n", payload.length());
    } else {
        DEBUG_PRINTF("[NetworkManager] HTTPS error: %d\n", code);
    }

    https.end();
    return payload;
}

// =============================================================================
// File Download
// =============================================================================

bool NetworkManager::downloadToSPIFFS(const String& url, const String& filePath) {
    if (!isConnected()) {
        DEBUG_PRINTLN("[NetworkManager] Download failed - not connected");
        return false;
    }

    DEBUG_PRINTF("[NetworkManager] Downloading %s to %s\n", url.c_str(), filePath.c_str());

    // Initialize SPIFFS if needed
    if (!SPIFFS.begin(true)) {
        DEBUG_PRINTLN("[NetworkManager] SPIFFS mount failed");
        return false;
    }

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(url);

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        DEBUG_PRINTF("[NetworkManager] Download HTTP error: %d\n", httpCode);
        http.end();
        return false;
    }

    // Get content length
    int contentLength = http.getSize();
    DEBUG_PRINTF("[NetworkManager] Content length: %d bytes\n", contentLength);

    // Open file for writing
    File file = SPIFFS.open(filePath, "w");
    if (!file) {
        DEBUG_PRINTLN("[NetworkManager] Failed to create file");
        http.end();
        return false;
    }

    // Stream data to file
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    int totalWritten = 0;

    while (http.connected() && (contentLength > 0 || contentLength == -1)) {
        size_t available = stream->available();
        if (available) {
            size_t readBytes = stream->readBytes(buffer, min(available, sizeof(buffer)));
            size_t written = file.write(buffer, readBytes);
            totalWritten += written;

            if (contentLength > 0) {
                contentLength -= readBytes;
            }
        }
        yield(); // Allow background tasks
    }

    file.close();
    http.end();

    DEBUG_PRINTF("[NetworkManager] Downloaded %d bytes\n", totalWritten);
    return totalWritten > 0;
}
