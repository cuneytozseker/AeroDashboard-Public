#ifndef TODOIST_MODULE_H
#define TODOIST_MODULE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SharedConfig.h>

// Forward declarations
class DisplayManager;
class ConfigManager;
class StateManager;

/**
 * Task structure for internal storage
 */
struct Task {
    String content;
    int priority;       // 1 = highest, 4 = lowest
    String dueString;   // Human-readable due date
    bool isOverdue;
};

/**
 * TodoistModule - Fetches and displays Todoist tasks
 *
 * Integrates with Todoist REST API to fetch tasks and render
 * them as a scrollable list with priority indicators.
 */
class TodoistModule {
public:
    enum class FetchResult {
        SUCCESS,
        WIFI_ERROR,
        HTTP_ERROR,
        PARSE_ERROR,
        AUTH_ERROR
    };

    TodoistModule();
    ~TodoistModule();

    // Initialize with configuration and state references
    void begin(ConfigManager* config, StateManager* state);

    // Data fetching
    FetchResult fetchTasks();

    // Load from cache (when offline)
    bool loadFromCache();

    // Render the Todoist module to display
    void render(DisplayManager& display, int x, int y, int width, int height);

    // Task accessors
    int getTaskCount() const { return _taskCount; }
    const Task* getTasks() const { return _tasks; }

private:
    // API communication
    String buildApiUrl() const;
    bool parseTasksJson(const String& json);

    // Render sub-components
    void renderHeader(DisplayManager& display, int x, int y, int width);
    void renderTaskList(DisplayManager& display, int x, int y, int width, int height);
    void renderTask(DisplayManager& display, int x, int y, int width, const Task& task, int index);
    void renderTimestamp(DisplayManager& display, int x, int y, bool isStale);
    void renderEmptyState(DisplayManager& display, int x, int y, int width, int height);

    // References to shared managers
    ConfigManager* _config;
    StateManager* _state;

    // Task storage
    Task* _tasks;
    int _taskCount;
    int _maxTasks;

    // Timestamps
    time_t _lastSync;
};

#endif // TODOIST_MODULE_H
