#ifndef WEATHER_MODULE_H
#define WEATHER_MODULE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SharedConfig.h>

// Forward declarations
class DisplayManager;
class ConfigManager;
class StateManager;

/**
 * WeatherModule - Fetches and displays weather data
 *
 * Integrates with OpenWeatherMap API for current conditions and
 * Turkish Meteorology radar images. Handles rendering of weather
 * section including radar, temperature, conditions, and history graph.
 */
class WeatherModule {
public:
    enum class FetchResult {
        SUCCESS,
        WIFI_ERROR,
        HTTP_ERROR,
        PARSE_ERROR,
        TIMEOUT
    };

    WeatherModule();

    // Initialize with configuration and state references
    void begin(ConfigManager* config, StateManager* state);

    // Data fetching
    FetchResult fetchWeatherData();
    FetchResult fetchForecast();
    bool fetchRadarImage();

    // Load from cache (when offline)
    bool loadFromCache();

    // Render the weather module to display
    void render(DisplayManager& display, int x, int y, int width, int height);

    // Current weather data accessors
    int getTemperature() const { return _temperature; }
    int getPressure() const { return _pressure; }
    int getWindGust() const { return _windGust; }
    int getWeatherCode() const { return _weatherCode; }
    float getDBZ() const { return _currentDBZ; }
    float getRain3h() const { return _rain3h; }
    String getConditionText() const;

private:
    // API data parsing
    bool parseWeatherJson(const String& json);
    bool parseForecastJson(const String& json);

    // Render sub-components
    void renderHeader(DisplayManager& display, int x, int y, int width);
    void renderRadar(DisplayManager& display, int x, int y, int size);
    void renderTemperature(DisplayManager& display, int x, int y);
    void renderGust(DisplayManager& display, int x, int y);
    void renderWeatherIcons(DisplayManager& display, int x, int y);
    void renderCondition(DisplayManager& display, int x, int y);

    // Helper functions
    void drawDitheredBitmap(DisplayManager& display, int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h);
    void renderStats(DisplayManager& display, int x, int y, int width);
    void renderGraph(DisplayManager& display, int x, int y, int width, int height);
    void renderForecast(DisplayManager& display, int x, int y, int width, int height);
    void renderSingleGraph(DisplayManager& display, int x, int y, int width, int height,
                           const int* data, int count, int minVal, int maxVal, const char* label, bool drawBorder = true);
    void renderTimestamp(DisplayManager& display, int x, int y, bool isStale);

    // References to shared managers
    ConfigManager* _config;
    StateManager* _state;

    // Current weather data
    int _temperature;       // Celsius
    int _pressure;          // hPa
    int _windGust;          // km/h
    int _weatherCode;       // OWM weather code
    float _currentDBZ;      // Radar reflectivity
    float _rain3h;          // Rain volume last 3 hours (mm)

    // Forecast data (4 days)
    struct ForecastDay {
        int tempMax;        // Max temperature (°C)
        int weatherCode;    // OWM weather code
    };
    ForecastDay _forecast[4];
    bool _hasForecast;

    // Radar processing state
    bool _radarAvailable;
    uint8_t* _radarBitmap;  // 1-bit dithered radar image
    int _radarSize;

    // Timestamps
    time_t _lastUpdate;
};

#endif // WEATHER_MODULE_H
