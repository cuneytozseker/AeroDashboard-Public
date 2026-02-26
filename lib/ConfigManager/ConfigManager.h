#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <SharedConfig.h>

/**
 * ConfigManager - Handles application configuration
 *
 * Loads configuration from SPIFFS JSON file and provides
 * access to all configuration values. Falls back to defaults
 * if file is missing or invalid.
 */
class ConfigManager {
public:
    ConfigManager();

    // Load configuration from SPIFFS
    bool load();

    // Load default values (fallback)
    void loadDefaults();

    // Save current configuration to SPIFFS
    bool save();

    // WiFi configuration
    const String& getWiFiSSID() const { return _wifiSSID; }
    const String& getWiFiPassword() const { return _wifiPassword; }

    // Weather API configuration
    const String& getOWMApiKey() const { return _owmApiKey; }
    float getLatitude() const { return _latitude; }
    float getLongitude() const { return _longitude; }
    const String& getUnits() const { return _units; }

    // Radar configuration
    const String& getRadarUrl() const { return _radarUrl; }
    int getRadarCropX() const { return _radarCropX; }
    int getRadarCropY() const { return _radarCropY; }
    int getRadarCropSize() const { return _radarCropSize; }

    // Todoist configuration
    const String& getTodoistToken() const { return _todoistToken; }
    const String& getTodoistFilter() const { return _todoistFilter; }
    int getMaxTasks() const { return _maxTasks; }

    // Display configuration
    int getRefreshInterval() const { return _refreshIntervalMinutes; }
    int getFullRefreshEveryN() const { return _fullRefreshEveryN; }
    int getTimezoneOffset() const { return _timezoneOffsetHours; }

    // Setters (for runtime modification)
    void setWiFiCredentials(const String& ssid, const String& password);
    void setOWMApiKey(const String& key);
    void setTodoistToken(const String& token);
    void setLocation(float lat, float lon);
    void setRefreshInterval(int minutes);
    void setTimezoneOffset(int hours);

    // Validation
    bool isValid() const;
    bool hasWiFiCredentials() const;
    bool hasWeatherApiKey() const;
    bool hasTodoistToken() const;

private:
    // Initialize SPIFFS
    bool initSPIFFS();

    // WiFi
    String _wifiSSID;
    String _wifiPassword;

    // Weather API
    String _owmApiKey;
    float _latitude;
    float _longitude;
    String _units;

    // Radar
    String _radarUrl;
    int _radarCropX;
    int _radarCropY;
    int _radarCropSize;

    // Todoist
    String _todoistToken;
    String _todoistFilter;
    int _maxTasks;

    // Display
    int _refreshIntervalMinutes;
    int _fullRefreshEveryN;
    int _timezoneOffsetHours;

    bool _loaded;
};

#endif // CONFIG_MANAGER_H
