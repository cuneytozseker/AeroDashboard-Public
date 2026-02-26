#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <SharedConfig.h>

/**
 * StateManager - Handles persistent state across deep sleep cycles
 *
 * Stores graph history data, cached API responses, and status flags.
 * Data is saved to SPIFFS to survive power cycles.
 */
class StateManager {
public:
    StateManager();

    // Load/save state from SPIFFS
    bool load();
    bool save();
    void reset();

    // Weather history (for 6-hour graph)
    void addWeatherHistoryPoint(int temp, int pressure, int gust, float dbz, float rain3h);
    int getHistoryCount() const { return _historyCount; }

    const int* getTemperatureHistory() const { return _tempHistory; }
    const int* getPressureHistory() const { return _pressureHistory; }
    const int* getGustHistory() const { return _gustHistory; }
    const float* getDBZHistory() const { return _dbzHistory; }
    const int* getRainHistory() const { return _rainHistory; }

    // Data freshness flags
    void setWeatherFresh(bool fresh) { _weatherFresh = fresh; }
    void setTodoistFresh(bool fresh) { _todoistFresh = fresh; }
    bool isWeatherFresh() const { return _weatherFresh; }
    bool isTodoistFresh() const { return _todoistFresh; }

    // Last update timestamps
    void setWeatherLastUpdate(time_t timestamp) { _weatherLastUpdate = timestamp; }
    void setTodoistLastUpdate(time_t timestamp) { _todoistLastUpdate = timestamp; }
    time_t getWeatherLastUpdate() const { return _weatherLastUpdate; }
    time_t getTodoistLastUpdate() const { return _todoistLastUpdate; }

    // Cached weather data
    void cacheWeatherData(int temp, int pressure, int gust, int weatherCode, float dbz);
    int getCachedTemperature() const { return _cachedTemp; }
    int getCachedPressure() const { return _cachedPressure; }
    int getCachedGust() const { return _cachedGust; }
    int getCachedWeatherCode() const { return _cachedWeatherCode; }
    float getCachedDBZ() const { return _cachedDBZ; }
    bool hasCachedWeather() const { return _hasCachedWeather; }

    // Cached Todoist tasks (stored as JSON string)
    void cacheTodoistTasks(const String& tasksJson);
    const String& getCachedTasks() const { return _cachedTasks; }
    bool hasCachedTasks() const { return _cachedTasks.length() > 0; }

    // Check if data is stale (older than threshold)
    bool isWeatherStale() const;
    bool isTodoistStale() const;

private:
    bool initSPIFFS();

    // History arrays for graph (36 points = 6 hours at 10-min intervals)
    int _tempHistory[GRAPH_HISTORY_POINTS];
    int _pressureHistory[GRAPH_HISTORY_POINTS];
    int _gustHistory[GRAPH_HISTORY_POINTS];
    float _dbzHistory[GRAPH_HISTORY_POINTS];
    int _rainHistory[GRAPH_HISTORY_POINTS];  // 3-hour rain accumulation in mm
    int _historyCount;

    // Freshness flags (current fetch status)
    bool _weatherFresh;
    bool _todoistFresh;

    // Last update timestamps
    time_t _weatherLastUpdate;
    time_t _todoistLastUpdate;

    // Cached weather values
    int _cachedTemp;
    int _cachedPressure;
    int _cachedGust;
    int _cachedWeatherCode;
    float _cachedDBZ;
    bool _hasCachedWeather;

    // Cached Todoist tasks (JSON string)
    String _cachedTasks;
};

#endif // STATE_MANAGER_H
