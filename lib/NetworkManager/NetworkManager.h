#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SharedConfig.h>

/**
 * NetworkManager - Handles WiFi connectivity and time synchronization
 *
 * Provides WiFi connection management, HTTP client helpers,
 * and NTP time synchronization.
 */
class NetworkManager {
public:
    NetworkManager();

    // WiFi management
    void setCredentials(const String& ssid, const String& password);
    bool connect(unsigned long timeoutMs = WIFI_CONNECT_TIMEOUT_MS);
    void disconnect();
    bool isConnected() const;

    // Connection info
    IPAddress getIPAddress() const;
    int8_t getRSSI() const;
    String getSSID() const;

    // Time synchronization
    bool syncTime(int timezoneOffsetHours = DEFAULT_TIMEZONE_OFFSET);
    time_t getEpochTime() const;
    String getFormattedTime() const;
    bool isTimeSynced() const { return _timeSynced; }

    // HTTP helpers (basic GET)
    String httpGet(const String& url, int* httpCode = nullptr);

    // HTTPS helpers (for Todoist API)
    String httpsGet(const String& url, const String& authHeader = "",
                    const char* rootCA = nullptr, int* httpCode = nullptr);

    // Download file to SPIFFS
    bool downloadToSPIFFS(const String& url, const String& filePath);

private:
    String _ssid;
    String _password;
    bool _timeSynced;

    WiFiUDP _ntpUDP;
    NTPClient* _timeClient;
};

#endif // NETWORK_MANAGER_H
