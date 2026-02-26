#include "TodoistModule.h"
#include "DisplayManager.h"
#include "ConfigManager.h"
#include "StateManager.h"
#include "../WeatherModule/Bitmaps.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

const char TODOIST_ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)EOF";

TodoistModule::TodoistModule()
    : _config(nullptr), _state(nullptr), _tasks(nullptr), _taskCount(0), _maxTasks(MAX_VISIBLE_TASKS), _lastSync(0)
{ _tasks = new Task[MAX_VISIBLE_TASKS]; }

TodoistModule::~TodoistModule() { if (_tasks) delete[] _tasks; }

void TodoistModule::begin(ConfigManager* config, StateManager* state) {
    _config = config; _state = state; _maxTasks = config->getMaxTasks();
}

String TodoistModule::buildApiUrl() const {
    String url = TODOIST_API_URL;
    String filter = _config->getTodoistFilter();
    if (filter.length() > 0) {
        filter.replace(" ", "%20"); filter.replace("|", "%7C"); filter.replace("#", "%23");
        url += "?filter=" + filter;
    }
    return url;
}

TodoistModule::FetchResult TodoistModule::fetchTasks() {
    LOG("[Todoist] fetchTasks() start");
    if (WiFi.status() != WL_CONNECTED) { LOG("[Todoist] WiFi not connected"); return FetchResult::WIFI_ERROR; }

    String url = buildApiUrl();
    LOGF("[Todoist] URL: %s", url.c_str());

    WiFiClientSecure client; char certBuffer[SSL_CERT_BUFFER_SIZE];
    memcpy_P(certBuffer, TODOIST_ROOT_CA, strlen_P(TODOIST_ROOT_CA) + 1);
    client.setCACert(certBuffer); client.setTimeout(HTTP_TIMEOUT_MS / 1000);

    HTTPClient https; https.setTimeout(HTTP_TIMEOUT_MS);
    if (!https.begin(client, url)) { LOG("[Todoist] https.begin() failed"); return FetchResult::HTTP_ERROR; }

    String token = _config->getTodoistToken();
    LOGF("[Todoist] Token length: %d", token.length());
    https.addHeader("Authorization", "Bearer " + token);
    https.addHeader("Content-Type", "application/json");

    LOG("[Todoist] Sending GET...");
    int httpCode = https.GET();
    LOGF("[Todoist] HTTP code: %d", httpCode);

    if (httpCode != HTTP_CODE_OK) {
        https.end();
        LOGF("[Todoist] Error body: %s", https.getString().substring(0, 100).c_str());
        FetchResult r = (httpCode == 401 || httpCode == 403) ? FetchResult::AUTH_ERROR : FetchResult::HTTP_ERROR;
        LOG(r == FetchResult::AUTH_ERROR ? "[Todoist] AUTH_ERROR" : "[Todoist] HTTP_ERROR");
        return r;
    }

    String payload = https.getString(); https.end();
    LOGF("[Todoist] Payload length: %d bytes", payload.length());
    LOGF("[Todoist] Payload preview: %.80s", payload.c_str());

    if (!parseTasksJson(payload)) { LOG("[Todoist] PARSE_ERROR"); return FetchResult::PARSE_ERROR; }

    LOGF("[Todoist] Parsed %d tasks", _taskCount);
    _lastSync = time(nullptr); _state->cacheTodoistTasks(payload); _state->setTodoistLastUpdate(_lastSync);
    LOG("[Todoist] fetchTasks() SUCCESS");
    return FetchResult::SUCCESS;
}

bool TodoistModule::parseTasksJson(const String& json) {
    JsonDocument doc; DeserializationError error = deserializeJson(doc, json);
    if (error) { LOGF("[Todoist] JSON parse error: %s", error.c_str()); return false; }
    _taskCount = 0;
    // API v1 wraps tasks in {"results": [...]}; v2 returned a bare array
    JsonArray tasksArray = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc["results"].as<JsonArray>();
    LOGF("[Todoist] tasksArray size: %d", tasksArray.size());
    for (JsonObject taskObj : tasksArray) {
        if (_taskCount >= _maxTasks) break;
        Task& task = _tasks[_taskCount];
        task.content = taskObj["content"] | "Untitled";
        int rawPriority = taskObj["priority"] | 1; task.priority = 5 - rawPriority;
        JsonObject due = taskObj["due"];
        if (due) {
            task.dueString = due["string"] | "";
            const char* dateStr = due["date"];
            if (dateStr) {
                time_t now = time(nullptr); struct tm* ti = localtime(&now); char todayStr[11];
                strftime(todayStr, sizeof(todayStr), "%Y-%m-%d", ti);
                task.isOverdue = (strcmp(dateStr, todayStr) < 0);
            } else task.isOverdue = false;
        } else { task.dueString = ""; task.isOverdue = false; }
        _taskCount++;
    }
    return true;
}

bool TodoistModule::loadFromCache() {
    if (!_state->hasCachedTasks()) { LOG("[Todoist] No cached tasks"); return false; }
    String cachedJson = _state->getCachedTasks();
    if (cachedJson.length() == 0) { LOG("[Todoist] Cache empty"); return false; }
    LOGF("[Todoist] Loading from cache (%d bytes)", cachedJson.length());
    _lastSync = _state->getTodoistLastUpdate();
    bool ok = parseTasksJson(cachedJson);
    LOGF("[Todoist] Cache parse %s, %d tasks", ok ? "OK" : "FAIL", _taskCount);
    return ok;
}

void TodoistModule::render(DisplayManager& display, int x, int y, int width, int height) {
    LOGF("[Todoist] render() taskCount=%d", _taskCount);
    renderHeader(display, x, y, width);
    if (_taskCount > 0) renderTaskList(display, x, y + 40, width, height - 40);
    else LOG("[Todoist] render: no tasks to display");
}

void TodoistModule::renderHeader(DisplayManager& display, int x, int y, int width) {
    // Header Text at absolute position (8, 67) - moved 2px left
    display.drawText(8, 64, "Todo List", 14, DisplayManager::BLACK, DisplayManager::FontType::Bold);
}

void TodoistModule::renderTaskList(DisplayManager& display, int x, int y, int width, int height) {
    // Start from TASK_LIST_Y (132 = 95 + 37)
    int currentY = TASK_LIST_Y;
    int lineHeight = TASK_LINE_HEIGHT;
    for (int i = 0; i < _taskCount && i < _maxTasks; i++) {
        if (currentY + lineHeight > y + height) break;
        renderTask(display, x, currentY, width, _tasks[i], i);
        currentY += lineHeight;
    }
}

void TodoistModule::renderTask(DisplayManager& display, int x, int y, int width, const Task& task, int index) {
    // Draw bullet bitmap at TASK_LIST_X + bullet offset, pushed down 2px
    int bulletX = TASK_LIST_X + 9;
    int bulletY = y + 2;  // Push bullet down 2px
    display.drawBitmap(bulletX, bulletY, gImage_bullet, 9, 9, DisplayManager::BLACK);

    // Overdue indicator
    if (task.isOverdue) {
        display.drawVerticalLine(bulletX + 4, bulletY - 3, 4);
        display.drawPixel(bulletX + 4, bulletY + 2, DisplayManager::BLACK);
    }
    // Task text at TASK_LIST_X + TASK_TEXT_OFFSET
    int textX = TASK_LIST_X + TASK_TEXT_OFFSET;
    String displayText = task.content;
    int16_t bx, by; uint16_t bw, bh;
    display.getTextBounds(displayText.c_str(), 14, &bx, &by, &bw, &bh, DisplayManager::FontType::Normal);
    if (bw > (width - 50)) {
        int maxChars = (width - 50) / 8;
        if (displayText.length() > maxChars) displayText = displayText.substring(0, maxChars) + "...";
    }
    DisplayManager::FontType fontType = (task.priority <= 2) ? DisplayManager::FontType::Bold : DisplayManager::FontType::Normal;
    display.drawText(textX, y, displayText.c_str(), 10, DisplayManager::BLACK, fontType);
}
