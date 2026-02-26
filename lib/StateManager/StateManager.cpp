#include "StateManager.h"

// State file structure version (increment when structure changes)
#define STATE_VERSION 1

// =============================================================================
// Constructor
// =============================================================================

StateManager::StateManager()
    : _historyCount(0)
    , _weatherFresh(false)
    , _todoistFresh(false)
    , _weatherLastUpdate(0)
    , _todoistLastUpdate(0)
    , _cachedTemp(0)
    , _cachedPressure(0)
    , _cachedGust(0)
    , _cachedWeatherCode(0)
    , _cachedDBZ(0)
    , _hasCachedWeather(false)
{
    // Initialize history arrays to zero
    memset(_tempHistory, 0, sizeof(_tempHistory));
    memset(_pressureHistory, 0, sizeof(_pressureHistory));
    memset(_gustHistory, 0, sizeof(_gustHistory));
    memset(_dbzHistory, 0, sizeof(_dbzHistory));
    memset(_rainHistory, 0, sizeof(_rainHistory));
}

// =============================================================================
// Load State from SPIFFS
// =============================================================================

bool StateManager::load() {
    DEBUG_PRINTLN("[StateManager] Loading state...");

    if (!initSPIFFS()) {
        return false;
    }

    if (!SPIFFS.exists(STATE_FILE_PATH)) {
        DEBUG_PRINTLN("[StateManager] State file not found");
        return false;
    }

    File stateFile = SPIFFS.open(STATE_FILE_PATH, "r");
    if (!stateFile) {
        DEBUG_PRINTLN("[StateManager] Failed to open state file");
        return false;
    }

    // Read and verify version
    uint8_t version;
    if (stateFile.read(&version, sizeof(version)) != sizeof(version)) {
        stateFile.close();
        return false;
    }

    if (version != STATE_VERSION) {
        DEBUG_PRINTF("[StateManager] Version mismatch (file: %d, expected: %d)\n",
                    version, STATE_VERSION);
        stateFile.close();
        return false;
    }

    // Read history count
    stateFile.read((uint8_t*)&_historyCount, sizeof(_historyCount));

    // Read history arrays
    stateFile.read((uint8_t*)_tempHistory, sizeof(_tempHistory));
    stateFile.read((uint8_t*)_pressureHistory, sizeof(_pressureHistory));
    stateFile.read((uint8_t*)_gustHistory, sizeof(_gustHistory));
    stateFile.read((uint8_t*)_dbzHistory, sizeof(_dbzHistory));
    stateFile.read((uint8_t*)_rainHistory, sizeof(_rainHistory));

    // Read timestamps
    stateFile.read((uint8_t*)&_weatherLastUpdate, sizeof(_weatherLastUpdate));
    stateFile.read((uint8_t*)&_todoistLastUpdate, sizeof(_todoistLastUpdate));

    // Read cached weather
    stateFile.read((uint8_t*)&_hasCachedWeather, sizeof(_hasCachedWeather));
    if (_hasCachedWeather) {
        stateFile.read((uint8_t*)&_cachedTemp, sizeof(_cachedTemp));
        stateFile.read((uint8_t*)&_cachedPressure, sizeof(_cachedPressure));
        stateFile.read((uint8_t*)&_cachedGust, sizeof(_cachedGust));
        stateFile.read((uint8_t*)&_cachedWeatherCode, sizeof(_cachedWeatherCode));
        stateFile.read((uint8_t*)&_cachedDBZ, sizeof(_cachedDBZ));
    }

    // Read cached tasks length and content
    uint16_t tasksLen;
    stateFile.read((uint8_t*)&tasksLen, sizeof(tasksLen));
    if (tasksLen > 0 && tasksLen < MAX_TASKS_JSON_SIZE) {
        char* tasksBuffer = (char*)malloc(tasksLen + 1);
        if (tasksBuffer) {
            stateFile.read((uint8_t*)tasksBuffer, tasksLen);
            tasksBuffer[tasksLen] = '\0';
            _cachedTasks = String(tasksBuffer);
            free(tasksBuffer);
        }
    }

    stateFile.close();

    DEBUG_PRINTF("[StateManager] State loaded (history: %d points)\n", _historyCount);
    return true;
}

// =============================================================================
// Save State to SPIFFS
// =============================================================================

bool StateManager::save() {
    DEBUG_PRINTLN("[StateManager] Saving state...");

    if (!initSPIFFS()) {
        return false;
    }

    File stateFile = SPIFFS.open(STATE_FILE_PATH, "w");
    if (!stateFile) {
        DEBUG_PRINTLN("[StateManager] Failed to create state file");
        return false;
    }

    // Write version
    uint8_t version = STATE_VERSION;
    stateFile.write(&version, sizeof(version));

    // Write history count
    stateFile.write((uint8_t*)&_historyCount, sizeof(_historyCount));

    // Write history arrays
    stateFile.write((uint8_t*)_tempHistory, sizeof(_tempHistory));
    stateFile.write((uint8_t*)_pressureHistory, sizeof(_pressureHistory));
    stateFile.write((uint8_t*)_gustHistory, sizeof(_gustHistory));
    stateFile.write((uint8_t*)_dbzHistory, sizeof(_dbzHistory));
    stateFile.write((uint8_t*)_rainHistory, sizeof(_rainHistory));

    // Write timestamps
    stateFile.write((uint8_t*)&_weatherLastUpdate, sizeof(_weatherLastUpdate));
    stateFile.write((uint8_t*)&_todoistLastUpdate, sizeof(_todoistLastUpdate));

    // Write cached weather
    stateFile.write((uint8_t*)&_hasCachedWeather, sizeof(_hasCachedWeather));
    if (_hasCachedWeather) {
        stateFile.write((uint8_t*)&_cachedTemp, sizeof(_cachedTemp));
        stateFile.write((uint8_t*)&_cachedPressure, sizeof(_cachedPressure));
        stateFile.write((uint8_t*)&_cachedGust, sizeof(_cachedGust));
        stateFile.write((uint8_t*)&_cachedWeatherCode, sizeof(_cachedWeatherCode));
        stateFile.write((uint8_t*)&_cachedDBZ, sizeof(_cachedDBZ));
    }

    // Write cached tasks
    uint16_t tasksLen = _cachedTasks.length();
    stateFile.write((uint8_t*)&tasksLen, sizeof(tasksLen));
    if (tasksLen > 0) {
        stateFile.write((uint8_t*)_cachedTasks.c_str(), tasksLen);
    }

    stateFile.close();

    DEBUG_PRINTLN("[StateManager] State saved");
    return true;
}

// =============================================================================
// Reset State
// =============================================================================

void StateManager::reset() {
    DEBUG_PRINTLN("[StateManager] Resetting state");

    _historyCount = 0;
    memset(_tempHistory, 0, sizeof(_tempHistory));
    memset(_pressureHistory, 0, sizeof(_pressureHistory));
    memset(_gustHistory, 0, sizeof(_gustHistory));
    memset(_dbzHistory, 0, sizeof(_dbzHistory));
    memset(_rainHistory, 0, sizeof(_rainHistory));

    _weatherFresh = false;
    _todoistFresh = false;
    _weatherLastUpdate = 0;
    _todoistLastUpdate = 0;

    _hasCachedWeather = false;
    _cachedTemp = 0;
    _cachedPressure = 0;
    _cachedGust = 0;
    _cachedWeatherCode = 0;
    _cachedDBZ = 0;

    _cachedTasks = "";
}

// =============================================================================
// History Management
// =============================================================================

void StateManager::addWeatherHistoryPoint(int temp, int pressure, int gust, float dbz, float rain3h) {
    // Shift history arrays left to make room for new point
    if (_historyCount >= GRAPH_HISTORY_POINTS) {
        // Array is full, shift everything left by 1
        for (int i = 0; i < GRAPH_HISTORY_POINTS - 1; i++) {
            _tempHistory[i] = _tempHistory[i + 1];
            _pressureHistory[i] = _pressureHistory[i + 1];
            _gustHistory[i] = _gustHistory[i + 1];
            _dbzHistory[i] = _dbzHistory[i + 1];
            _rainHistory[i] = _rainHistory[i + 1];
        }
        // Add new point at the end
        _tempHistory[GRAPH_HISTORY_POINTS - 1] = temp;
        _pressureHistory[GRAPH_HISTORY_POINTS - 1] = pressure;
        _gustHistory[GRAPH_HISTORY_POINTS - 1] = gust;
        _dbzHistory[GRAPH_HISTORY_POINTS - 1] = dbz;
        _rainHistory[GRAPH_HISTORY_POINTS - 1] = (int)rain3h;  // Convert float to int mm
    } else {
        // Still have room, add at current position
        _tempHistory[_historyCount] = temp;
        _pressureHistory[_historyCount] = pressure;
        _gustHistory[_historyCount] = gust;
        _dbzHistory[_historyCount] = dbz;
        _rainHistory[_historyCount] = (int)rain3h;  // Convert float to int mm
        _historyCount++;
    }

    DEBUG_PRINTF("[StateManager] Added history point (total: %d)\n", _historyCount);
}

// =============================================================================
// Cache Management
// =============================================================================

void StateManager::cacheWeatherData(int temp, int pressure, int gust, int weatherCode, float dbz) {
    _cachedTemp = temp;
    _cachedPressure = pressure;
    _cachedGust = gust;
    _cachedWeatherCode = weatherCode;
    _cachedDBZ = dbz;
    _hasCachedWeather = true;
    _weatherLastUpdate = time(nullptr);
}

void StateManager::cacheTodoistTasks(const String& tasksJson) {
    _cachedTasks = tasksJson;
    _todoistLastUpdate = time(nullptr);
}

// =============================================================================
// Staleness Check
// =============================================================================

bool StateManager::isWeatherStale() const {
    if (_weatherLastUpdate == 0) return true;

    time_t now = time(nullptr);
    int minutesSinceUpdate = (now - _weatherLastUpdate) / 60;

    return minutesSinceUpdate > STALE_THRESHOLD_MINUTES;
}

bool StateManager::isTodoistStale() const {
    if (_todoistLastUpdate == 0) return true;

    time_t now = time(nullptr);
    int minutesSinceUpdate = (now - _todoistLastUpdate) / 60;

    return minutesSinceUpdate > STALE_THRESHOLD_MINUTES;
}

// =============================================================================
// Private Methods
// =============================================================================

bool StateManager::initSPIFFS() {
    static bool initialized = false;

    if (initialized) {
        return true;
    }

    if (!SPIFFS.begin(true)) {
        DEBUG_PRINTLN("[StateManager] SPIFFS mount failed");
        return false;
    }

    initialized = true;
    return true;
}
