#include <Arduino.h>
#include <SharedConfig.h>
#include <Pins.h>
#include "DisplayManager.h"
#include "ConfigManager.h"
#include "StateManager.h"
#include "NetworkManager.h"
#include "OTAManager.h"
#include "WeatherModule.h"
#include "TodoistModule.h"
#include "DebugServer.h"
#include <Fonts/Picopixel.h>

// Global log buffer definition (declared extern in SharedConfig.h)
LogBuffer gLogBuffer = {};

DisplayManager  display;
ConfigManager   config;
StateManager    state;
NetworkManager  network;
OTAManager      otaManager;
WeatherModule   weather;
TodoistModule   todoist;
DebugServer     debugServer;

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int updatesSinceFullRefresh = 0;

void initializeSystem();
void fetchAllData();
void renderDisplay();
void enterDeepSleep();
bool shouldDoFullRefresh();

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(100);
    bootCount++;
    initializeSystem();
    fetchAllData();
    renderDisplay();
    state.save();
    DEBUG_PRINTLN("\n[INFO] Deep sleep disabled - running in continuous mode");
}

void loop() {
    otaManager.handle();
    debugServer.handleClient();
    static unsigned long lastUpdate = 0;
    unsigned long refreshInterval = REFRESH_INTERVAL_MINUTES * 60 * 1000UL;
    if (millis() - lastUpdate >= refreshInterval) {
        fetchAllData();
        renderDisplay();
        state.save();
        lastUpdate = millis();
    }
    delay(100);
}

void initializeSystem() {
    display.begin();
    if (!config.load()) config.loadDefaults();
    if (!state.load()) state.reset();
    network.setCredentials(config.getWiFiSSID(), config.getWiFiPassword());
    if (network.connect(WIFI_CONNECT_TIMEOUT_MS)) {
        network.syncTime(config.getTimezoneOffset());
        otaManager.begin(OTA_HOSTNAME, OTA_PASSWORD, OTA_PORT);
        debugServer.begin(&display);
    }
    weather.begin(&config, &state);
    todoist.begin(&config, &state);
}

void fetchAllData() {
    bool wifiConnected = network.isConnected();
    if (wifiConnected) {
        if (weather.fetchWeatherData() == WeatherModule::FetchResult::SUCCESS) state.setWeatherFresh(true);
        else state.setWeatherFresh(false);
        weather.fetchForecast();
        weather.fetchRadarImage();
        if (todoist.fetchTasks() == TodoistModule::FetchResult::SUCCESS) state.setTodoistFresh(true);
        else state.setTodoistFresh(false);
    } else {
        state.setWeatherFresh(false);
        weather.loadFromCache();
        todoist.loadFromCache();
    }
    if (state.isWeatherFresh()) {
        state.addWeatherHistoryPoint(weather.getTemperature(), weather.getPressure(), weather.getWindGust(), weather.getDBZ(), weather.getRain3h());
    }
}

void renderDisplay() {
    if (shouldDoFullRefresh()) {
        display.setFullRefreshMode();
        updatesSinceFullRefresh = 0;
    } else {
        display.setPartialRefreshMode();
        updatesSinceFullRefresh++;
    }
    display.beginRender();
    display.clear();

    // Background layout
    display.drawBackground("/bg.bin");

    // Global Header
    display.drawText(10, 10, "AERODASH", 18, DisplayManager::BLACK, DisplayManager::FontType::Bold);

    // Modules
    weather.render(display, WEATHER_MODULE_X, 0, WEATHER_MODULE_WIDTH, DISPLAY_HEIGHT);
    todoist.render(display, TODOIST_MODULE_X, 0, TODOIST_MODULE_WIDTH, DISPLAY_HEIGHT);

    // Status bar - using Picopixel font (7 pixels high)
    display.setFont(&Picopixel);
    display.setTextColor(DisplayManager::WHITE);
    display.setTextSize(1);

    // WiFi signal strength at 97,261
    int8_t rssi = network.getRSSI();
    display.setCursor(97, 261);
    display.print("WIFI: ");
    if (network.isConnected()) {
        display.print(rssi);
        display.print("dBm");
    } else {
        display.print("OFF");
    }

    display.endRender();
    display.hibernate();
}

void enterDeepSleep() {
    int sleepSeconds = (state.isWeatherFresh() || state.isTodoistFresh()) ? config.getRefreshInterval() * 60 : OFFLINE_RETRY_SECONDS;
    network.disconnect();
    esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
    esp_deep_sleep_start();
}

bool shouldDoFullRefresh() {
    if (bootCount == 1) return true;
    if (updatesSinceFullRefresh >= FULL_REFRESH_EVERY_N) return true;
    return false;
}
