#include "ConfigManager.h"

// =============================================================================
// Constructor
// =============================================================================

ConfigManager::ConfigManager()
    : _loaded(false)
{
    loadDefaults();
}

// =============================================================================
// Load Configuration from SPIFFS
// =============================================================================

bool ConfigManager::load() {
    DEBUG_PRINTLN("[ConfigManager] Loading configuration...");

    if (!initSPIFFS()) {
        DEBUG_PRINTLN("[ConfigManager] SPIFFS init failed");
        return false;
    }

    // Check if config file exists
    if (!SPIFFS.exists(CONFIG_FILE_PATH)) {
        DEBUG_PRINTLN("[ConfigManager] Config file not found, using defaults");
        return false;
    }

    // Open config file
    File configFile = SPIFFS.open(CONFIG_FILE_PATH, "r");
    if (!configFile) {
        DEBUG_PRINTLN("[ConfigManager] Failed to open config file");
        return false;
    }

    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();

    if (error) {
        DEBUG_PRINTF("[ConfigManager] JSON parse error: %s\n", error.c_str());
        return false;
    }

    // Extract WiFi settings
    JsonObject wifi = doc["wifi"];
    if (wifi) {
        _wifiSSID = wifi["ssid"] | DEFAULT_WIFI_SSID;
        _wifiPassword = wifi["password"] | DEFAULT_WIFI_PASSWORD;
    }

    // Extract Weather settings
    JsonObject weather = doc["weather"];
    if (weather) {
        _owmApiKey = weather["api_key"] | DEFAULT_OWM_API_KEY;
        _latitude = weather["latitude"] | DEFAULT_LATITUDE;
        _longitude = weather["longitude"] | DEFAULT_LONGITUDE;
        _units = weather["units"] | DEFAULT_UNITS;
        _radarUrl = weather["radar_url"] | DEFAULT_RADAR_URL;
        _radarCropX = weather["crop_x"] | RADAR_CROP_X;
        _radarCropY = weather["crop_y"] | RADAR_CROP_Y;
        _radarCropSize = weather["crop_size"] | RADAR_CROP_SIZE;
    }

    // Extract Todoist settings
    JsonObject todoist = doc["todoist"];
    if (todoist) {
        _todoistToken = todoist["api_token"] | DEFAULT_TODOIST_TOKEN;
        _todoistFilter = todoist["filter"] | DEFAULT_TODOIST_FILTER;
        _maxTasks = todoist["max_tasks"] | DEFAULT_MAX_TASKS;
    }

    // Extract Display settings
    JsonObject display = doc["display"];
    if (display) {
        _refreshIntervalMinutes = display["refresh_interval_minutes"] | REFRESH_INTERVAL_MINUTES;
        _fullRefreshEveryN = display["full_refresh_every_n"] | FULL_REFRESH_EVERY_N;
        _timezoneOffsetHours = display["timezone_offset_hours"] | DEFAULT_TIMEZONE_OFFSET;
    }

    _loaded = true;
    DEBUG_PRINTLN("[ConfigManager] Configuration loaded successfully");

    return true;
}

// =============================================================================
// Load Default Values
// =============================================================================

void ConfigManager::loadDefaults() {
    DEBUG_PRINTLN("[ConfigManager] Loading default values");

    // WiFi
    _wifiSSID = DEFAULT_WIFI_SSID;
    _wifiPassword = DEFAULT_WIFI_PASSWORD;

    // Weather API
    _owmApiKey = DEFAULT_OWM_API_KEY;
    _latitude = DEFAULT_LATITUDE;
    _longitude = DEFAULT_LONGITUDE;
    _units = DEFAULT_UNITS;

    // Radar
    _radarUrl = DEFAULT_RADAR_URL;
    _radarCropX = RADAR_CROP_X;
    _radarCropY = RADAR_CROP_Y;
    _radarCropSize = RADAR_CROP_SIZE;

    // Todoist
    _todoistToken = DEFAULT_TODOIST_TOKEN;
    _todoistFilter = DEFAULT_TODOIST_FILTER;
    _maxTasks = DEFAULT_MAX_TASKS;

    // Display
    _refreshIntervalMinutes = REFRESH_INTERVAL_MINUTES;
    _fullRefreshEveryN = FULL_REFRESH_EVERY_N;
    _timezoneOffsetHours = DEFAULT_TIMEZONE_OFFSET;

    _loaded = true;
}

// =============================================================================
// Save Configuration to SPIFFS
// =============================================================================

bool ConfigManager::save() {
    DEBUG_PRINTLN("[ConfigManager] Saving configuration...");

    if (!initSPIFFS()) {
        DEBUG_PRINTLN("[ConfigManager] SPIFFS init failed");
        return false;
    }

    // Create JSON document
    JsonDocument doc;

    // WiFi section
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = _wifiSSID;
    wifi["password"] = _wifiPassword;

    // Weather section
    JsonObject weather = doc["weather"].to<JsonObject>();
    weather["api_key"] = _owmApiKey;
    weather["latitude"] = _latitude;
    weather["longitude"] = _longitude;
    weather["units"] = _units;
    weather["radar_url"] = _radarUrl;
    weather["crop_x"] = _radarCropX;
    weather["crop_y"] = _radarCropY;
    weather["crop_size"] = _radarCropSize;

    // Todoist section
    JsonObject todoist = doc["todoist"].to<JsonObject>();
    todoist["api_token"] = _todoistToken;
    todoist["filter"] = _todoistFilter;
    todoist["max_tasks"] = _maxTasks;

    // Display section
    JsonObject display = doc["display"].to<JsonObject>();
    display["refresh_interval_minutes"] = _refreshIntervalMinutes;
    display["full_refresh_every_n"] = _fullRefreshEveryN;
    display["timezone_offset_hours"] = _timezoneOffsetHours;

    // Write to file
    File configFile = SPIFFS.open(CONFIG_FILE_PATH, "w");
    if (!configFile) {
        DEBUG_PRINTLN("[ConfigManager] Failed to create config file");
        return false;
    }

    if (serializeJsonPretty(doc, configFile) == 0) {
        DEBUG_PRINTLN("[ConfigManager] Failed to write config file");
        configFile.close();
        return false;
    }

    configFile.close();
    DEBUG_PRINTLN("[ConfigManager] Configuration saved successfully");

    return true;
}

// =============================================================================
// Setters
// =============================================================================

void ConfigManager::setWiFiCredentials(const String& ssid, const String& password) {
    _wifiSSID = ssid;
    _wifiPassword = password;
}

void ConfigManager::setOWMApiKey(const String& key) {
    _owmApiKey = key;
}

void ConfigManager::setTodoistToken(const String& token) {
    _todoistToken = token;
}

void ConfigManager::setLocation(float lat, float lon) {
    _latitude = lat;
    _longitude = lon;
}

void ConfigManager::setRefreshInterval(int minutes) {
    _refreshIntervalMinutes = minutes;
}

void ConfigManager::setTimezoneOffset(int hours) {
    _timezoneOffsetHours = hours;
}

// =============================================================================
// Validation
// =============================================================================

bool ConfigManager::isValid() const {
    return hasWiFiCredentials() && (hasWeatherApiKey() || hasTodoistToken());
}

bool ConfigManager::hasWiFiCredentials() const {
    return _wifiSSID.length() > 0 && _wifiPassword.length() > 0;
}

bool ConfigManager::hasWeatherApiKey() const {
    return _owmApiKey.length() > 0;
}

bool ConfigManager::hasTodoistToken() const {
    return _todoistToken.length() > 0;
}

// =============================================================================
// Private Methods
// =============================================================================

bool ConfigManager::initSPIFFS() {
    static bool initialized = false;

    if (initialized) {
        return true;
    }

    if (!SPIFFS.begin(true)) { // true = format if mount fails
        DEBUG_PRINTLN("[ConfigManager] SPIFFS mount failed");
        return false;
    }

    initialized = true;
    return true;
}
