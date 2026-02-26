#ifndef SHARED_CONFIG_H
#define SHARED_CONFIG_H

#include <Arduino.h>

// =============================================================================
// AeroDashboard Shared Configuration
// =============================================================================
// This file contains all shared constants and macros that need to be
// accessible across all library modules.
// =============================================================================

// -----------------------------------------------------------------------------
// Display Configuration
// -----------------------------------------------------------------------------
#define DISPLAY_WIDTH           792
#define DISPLAY_HEIGHT          272

// Module boundaries - SWAPPED: Todoist (Left) | Weather (Right)
#define TODOIST_MODULE_X        0
#define TODOIST_MODULE_WIDTH    392

#define WEATHER_MODULE_X        392
#define WEATHER_MODULE_WIDTH    400

// Refresh settings
#define REFRESH_INTERVAL_MINUTES    10
#define FULL_REFRESH_EVERY_N        6

// -----------------------------------------------------------------------------
// Global Layout
// -----------------------------------------------------------------------------
#define HEADER_HEIGHT           60
#define MODULE_START_Y          95

// -----------------------------------------------------------------------------
// Weather Module Layout
// -----------------------------------------------------------------------------
#define WEATHER_HEADER_Y        20
#define RADAR_X                 254
#define RADAR_Y                 112
#define RADAR_DISPLAY_SIZE      140

#define TEMP_X                  200
#define TEMP_Y                  80

#define CONDITION_X             200
#define CONDITION_Y             130

#define STATS_Y                 250

// Vitals / Graph (Bottom)
#define VITALS_LABEL_Y          200
#define GRAPH_X                 0
#define GRAPH_Y                 210
#define GRAPH_WIDTH             220
#define GRAPH_HEIGHT            50
#define GRAPH_HISTORY_POINTS    36

// -----------------------------------------------------------------------------
// Todoist Module Layout (Relative to TODOIST_MODULE_X)
// -----------------------------------------------------------------------------
#define TASK_HEADER_Y           MODULE_START_Y
#define TASK_LIST_X             1
#define TASK_LIST_Y             106 // Original was 94, moved down 12px
#define TASK_LINE_HEIGHT        24
#define TASK_BULLET_RADIUS      3
#define TASK_TEXT_OFFSET        22
#define MAX_VISIBLE_TASKS       6

// -----------------------------------------------------------------------------
// Weather API Configuration (OpenWeatherMap)
// -----------------------------------------------------------------------------
#define DEFAULT_OWM_API_KEY     ""
#define DEFAULT_LATITUDE        0.0     // Set your latitude in data/config.json
#define DEFAULT_LONGITUDE       0.0     // Set your longitude in data/config.json
#define DEFAULT_UNITS           "metric"

// Radar image source
#define DEFAULT_RADAR_URL       "https://www.mgm.gov.tr/FTPDATA/uzal/radar/ist/istmax15.jpg"
#define RADAR_CROP_X            390
#define RADAR_CROP_Y            420
#define RADAR_CROP_SIZE         39

// -----------------------------------------------------------------------------
// Todoist API Configuration
// -----------------------------------------------------------------------------
#define DEFAULT_TODOIST_TOKEN   ""
#define DEFAULT_TODOIST_FILTER  "today | overdue"
#define DEFAULT_MAX_TASKS       9

// Todoist API endpoint
#define TODOIST_API_URL         "https://api.todoist.com/api/v1/tasks"

// -----------------------------------------------------------------------------
// Network Configuration
// -----------------------------------------------------------------------------
#define DEFAULT_WIFI_SSID       ""
#define DEFAULT_WIFI_PASSWORD   ""

#define WIFI_CONNECT_TIMEOUT_MS     30000
#define HTTP_TIMEOUT_MS             15000
#define NTP_UPDATE_INTERVAL_MS      3600000

// NTP server
#define NTP_SERVER              "pool.ntp.org"
#define DEFAULT_TIMEZONE_OFFSET 3

// -----------------------------------------------------------------------------
// OTA Update Configuration
// -----------------------------------------------------------------------------
#define OTA_HOSTNAME            "aerodashboard"
#define OTA_PASSWORD            "your_ota_password"
#define OTA_PORT                3232

// -----------------------------------------------------------------------------
// Stale Data Thresholds
// -----------------------------------------------------------------------------
#define STALE_THRESHOLD_MINUTES     30
#define OFFLINE_RETRY_SECONDS       60

// -----------------------------------------------------------------------------
// Memory Configuration
// -----------------------------------------------------------------------------
#define JSON_DOC_SIZE               8192
#define MAX_TASKS_JSON_SIZE         8192
#define SSL_CERT_BUFFER_SIZE        2048

// -----------------------------------------------------------------------------
// File Paths (SPIFFS)
// -----------------------------------------------------------------------------
#define CONFIG_FILE_PATH        "/config.json"
#define STATE_FILE_PATH         "/state.dat"
#define RADAR_CACHE_PATH        "/radar.jpg"

// -----------------------------------------------------------------------------
// Debug Configuration
// -----------------------------------------------------------------------------
#define SERIAL_BAUD_RATE        115200

// Define ENABLE_DEBUG_OUTPUT in platformio.ini build_flags if you want debug
#ifndef ENABLE_DEBUG_OUTPUT
#define ENABLE_DEBUG_OUTPUT     1
#endif

// -----------------------------------------------------------------------------
// In-memory log ring buffer (accessible via /logs endpoint)
// -----------------------------------------------------------------------------
#define LOG_BUFFER_LINES    64
#define LOG_LINE_MAX_LEN    120

struct LogBuffer {
    char lines[LOG_BUFFER_LINES][LOG_LINE_MAX_LEN];
    int  head;   // next write position
    int  count;  // total lines stored (capped at LOG_BUFFER_LINES)
};

extern LogBuffer gLogBuffer;

inline void logWrite(const char* msg) {
    int idx = gLogBuffer.head % LOG_BUFFER_LINES;
    strncpy(gLogBuffer.lines[idx], msg, LOG_LINE_MAX_LEN - 1);
    gLogBuffer.lines[idx][LOG_LINE_MAX_LEN - 1] = '\0';
    gLogBuffer.head = (gLogBuffer.head + 1) % LOG_BUFFER_LINES;
    if (gLogBuffer.count < LOG_BUFFER_LINES) gLogBuffer.count++;
}

inline void logWritef(const char* fmt, ...) {
    char buf[LOG_LINE_MAX_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    logWrite(buf);
}

#define LOG(msg)            do { logWrite(msg);        Serial.println(msg); } while(0)
#define LOGF(fmt, ...)      do { logWritef(fmt, ##__VA_ARGS__); Serial.printf(fmt "\n", ##__VA_ARGS__); } while(0)

#if ENABLE_DEBUG_OUTPUT
    #define DEBUG_PRINT(x)      Serial.print(x)
    #define DEBUG_PRINTLN(x)    Serial.println(x)
    #define DEBUG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

// -----------------------------------------------------------------------------
// Debug Server Configuration
// -----------------------------------------------------------------------------
// Enable debug web server with screenshot capability
// Set to 0 to completely remove debug server and shadow buffer (saves 27KB RAM + 7KB flash)
#ifndef ENABLE_DEBUG_SERVER
#define ENABLE_DEBUG_SERVER     1
#endif

#endif // SHARED_CONFIG_H
