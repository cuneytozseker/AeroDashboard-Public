#include "WeatherModule.h"
#include "DisplayManager.h"
#include "ConfigManager.h"
#include "StateManager.h"
#include "RadarProcessor.h"
#include "GraphRenderer.h"
#include "Bitmaps.h"
#include "WeatherIcons.h"
#include <HTTPClient.h>
#include <WiFi.h>

WeatherModule::WeatherModule()
    : _config(nullptr), _state(nullptr), _temperature(0), _pressure(0), _windGust(0), _weatherCode(0), _currentDBZ(0), _rain3h(0), _radarAvailable(false), _radarBitmap(nullptr), _radarSize(0), _lastUpdate(0), _hasForecast(false)
{
    // Initialize forecast data
    for (int i = 0; i < 4; i++) {
        _forecast[i].tempMax = 0;
        _forecast[i].weatherCode = 0;
    }
}

void WeatherModule::begin(ConfigManager* config, StateManager* state) {
    _config = config; _state = state;
}

WeatherModule::FetchResult WeatherModule::fetchWeatherData() {
    if (WiFi.status() != WL_CONNECTED) return FetchResult::WIFI_ERROR;
    String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(_config->getLatitude(), 6) + "&lon=" + String(_config->getLongitude(), 6) + "&appid=" + _config->getOWMApiKey() + "&units=" + _config->getUnits();
    HTTPClient http; http.setTimeout(HTTP_TIMEOUT_MS); http.begin(url);
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) { http.end(); return FetchResult::HTTP_ERROR; }
    String payload = http.getString(); http.end();
    if (!parseWeatherJson(payload)) return FetchResult::PARSE_ERROR;
    _lastUpdate = time(nullptr);
    _state->cacheWeatherData(_temperature, _pressure, _windGust, _weatherCode, _currentDBZ);
    _state->setWeatherLastUpdate(_lastUpdate);
    DEBUG_PRINTF("[Weather] Parsed: Temp=%d°C, Pressure=%dhPa, Gust=%dkm/h\n", _temperature, _pressure, _windGust);
    return FetchResult::SUCCESS;
}

WeatherModule::FetchResult WeatherModule::fetchForecast() {
    if (WiFi.status() != WL_CONNECTED) return FetchResult::WIFI_ERROR;

    // OpenWeatherMap 5-day/3-hour forecast API
    String url = "http://api.openweathermap.org/data/2.5/forecast?lat=" +
                 String(_config->getLatitude(), 6) + "&lon=" +
                 String(_config->getLongitude(), 6) + "&appid=" +
                 _config->getOWMApiKey() + "&units=" + _config->getUnits();

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return FetchResult::HTTP_ERROR;
    }

    String payload = http.getString();
    http.end();

    if (!parseForecastJson(payload)) {
        return FetchResult::PARSE_ERROR;
    }

    DEBUG_PRINTF("[Weather] Forecast parsed for 4 days\n");
    return FetchResult::SUCCESS;
}

bool WeatherModule::fetchRadarImage() {
    HTTPClient http; http.setTimeout(HTTP_TIMEOUT_MS); http.begin(_config->getRadarUrl());
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) { http.end(); _radarAvailable = false; return false; }
    WiFiClient* stream = http.getStreamPtr();
    File file = SPIFFS.open(RADAR_CACHE_PATH, "w");
    if (!file) { http.end(); _radarAvailable = false; return false; }
    uint8_t buffer[1024];
    while (http.connected() && stream->available()) {
        size_t bytes = stream->readBytes(buffer, sizeof(buffer));
        file.write(buffer, bytes);
        yield();
    }
    file.close(); http.end();
    RadarProcessor processor;
    _radarBitmap = processor.processRadarImage(RADAR_CACHE_PATH, _config->getRadarCropX(), _config->getRadarCropY(), _config->getRadarCropSize(), RADAR_DISPLAY_SIZE, &_currentDBZ);
    if (_radarBitmap) { _radarSize = RADAR_DISPLAY_SIZE; _radarAvailable = true; return true; }
    _radarAvailable = false; return false;
}

bool WeatherModule::loadFromCache() {
    if (!_state->hasCachedWeather()) return false;
    _temperature = _state->getCachedTemperature();
    _pressure = _state->getCachedPressure();
    _windGust = _state->getCachedGust();
    _weatherCode = _state->getCachedWeatherCode();
    _currentDBZ = _state->getCachedDBZ();
    _lastUpdate = _state->getWeatherLastUpdate();
    return true;
}

bool WeatherModule::parseWeatherJson(const String& json) {
    // Parse temperature
    int tempIndex = json.indexOf("\"temp\":");
    if (tempIndex >= 0) {
        int endIndex = json.indexOf(',', tempIndex);
        _temperature = (int)round(json.substring(tempIndex + 7, endIndex).toFloat());
    } else return false;

    // Parse weather code
    int idIndex = json.indexOf("\"id\":");
    if (idIndex >= 0) {
        int endIndex = json.indexOf(',', idIndex);
        _weatherCode = json.substring(idIndex + 5, endIndex).toInt();
    }

    // Parse pressure (in "main" section)
    int pressureIndex = json.indexOf("\"pressure\":");
    if (pressureIndex >= 0) {
        int endIndex = json.indexOf(',', pressureIndex);
        if (endIndex < 0) endIndex = json.indexOf('}', pressureIndex);
        _pressure = json.substring(pressureIndex + 11, endIndex).toInt();
    }

    // Parse wind gust (in "wind" section)
    // OpenWeatherMap provides: "wind":{"speed":X.X,"deg":X,"gust":X.X}
    int gustIndex = json.indexOf("\"gust\":");
    if (gustIndex >= 0) {
        int endIndex = json.indexOf(',', gustIndex);
        if (endIndex < 0) endIndex = json.indexOf('}', gustIndex);
        float gustMs = json.substring(gustIndex + 7, endIndex).toFloat();
        _windGust = (int)round(gustMs * 3.6); // Convert m/s to km/h
    } else {
        // If gust not available, use wind speed as fallback
        int speedIndex = json.indexOf("\"speed\":");
        if (speedIndex >= 0) {
            int endIndex = json.indexOf(',', speedIndex);
            if (endIndex < 0) endIndex = json.indexOf('}', speedIndex);
            float speedMs = json.substring(speedIndex + 8, endIndex).toFloat();
            _windGust = (int)round(speedMs * 3.6); // Convert m/s to km/h
        }
    }

    // Parse rain (in "rain" section)
    // OpenWeatherMap provides: "rain":{"3h":X.X}
    int rain3hIndex = json.indexOf("\"rain\":");
    if (rain3hIndex >= 0) {
        int threeHIndex = json.indexOf("\"3h\":", rain3hIndex);
        if (threeHIndex >= 0) {
            int endIndex = json.indexOf(',', threeHIndex);
            if (endIndex < 0) endIndex = json.indexOf('}', threeHIndex);
            _rain3h = json.substring(threeHIndex + 5, endIndex).toFloat();
        }
    } else {
        _rain3h = 0;
    }

    return true;
}

bool WeatherModule::parseForecastJson(const String& json) {
    // Simplified: Extract temp and weather from 4 forecast entries at 12:00 (noon)
    // This gives us one data point per day

    _hasForecast = false;

    // Find the "list" array
    int listStart = json.indexOf("\"list\":[");
    if (listStart < 0) {
        DEBUG_PRINTLN("[Weather] ERROR: 'list' not found");
        return false;
    }

    int pos = listStart + 8;
    int dayCount = 0;
    int entriesChecked = 0;

    // Look through forecast entries
    while (pos < json.length() && dayCount < 4 && entriesChecked < 40) {
        entriesChecked++;

        // Find next forecast object
        int objStart = json.indexOf('{', pos);
        if (objStart < 0) break;

        // Find the closing brace for this object (approximate - find next one)
        int objEnd = json.indexOf("},{", objStart);
        if (objEnd < 0) objEnd = json.indexOf("}]", objStart);
        if (objEnd < 0) break;

        // Extract substring for this forecast entry
        String entry = json.substring(objStart, objEnd + 1);

        // Check if this is a noon forecast (12:00:00) - more reliable for daily max
        // dt_txt format: "2024-02-11 12:00:00"
        if (entry.indexOf("12:00:00") >= 0 || dayCount == 0) {
            // Parse temp from "main":{"temp":X.X
            int tempIndex = entry.indexOf("\"temp\":");
            if (tempIndex >= 0) {
                int tempEnd = entry.indexOf(',', tempIndex);
                if (tempEnd < 0) tempEnd = entry.indexOf('}', tempIndex);
                float temp = entry.substring(tempIndex + 7, tempEnd).toFloat();
                _forecast[dayCount].tempMax = (int)round(temp);

                // Parse weather code from "weather":[{"id":X
                int weatherIndex = entry.indexOf("\"weather\":[");
                if (weatherIndex >= 0) {
                    int idIndex = entry.indexOf("\"id\":", weatherIndex);
                    if (idIndex >= 0) {
                        int idEnd = entry.indexOf(',', idIndex);
                        if (idEnd < 0) idEnd = entry.indexOf('}', idIndex);
                        _forecast[dayCount].weatherCode = entry.substring(idIndex + 5, idEnd).toInt();
                    }
                }

                DEBUG_PRINTF("[Weather] Day %d: Temp=%d, Code=%d\n",
                             dayCount + 1, _forecast[dayCount].tempMax, _forecast[dayCount].weatherCode);

                dayCount++;
            }
        }

        pos = objEnd + 1;
    }

    _hasForecast = (dayCount >= 4);

    if (_hasForecast) {
        DEBUG_PRINTF("[Weather] Forecast OK: %d days parsed\n", dayCount);
    } else {
        DEBUG_PRINTF("[Weather] Forecast FAIL: Only %d days found\n", dayCount);
    }

    return _hasForecast;
}

String WeatherModule::getConditionText() const {
    if (_weatherCode >= 200 && _weatherCode < 300) return "Thunderstorm";
    if (_weatherCode >= 300 && _weatherCode < 400) return "Drizzle";
    if (_weatherCode >= 500 && _weatherCode < 600) return "Rain";
    if (_weatherCode >= 600 && _weatherCode < 700) return "Snow";
    if (_weatherCode >= 700 && _weatherCode < 800) return "Fog";
    if (_weatherCode == 800) return "Clear";
    if (_weatherCode == 801) return "Few Clouds";
    if (_weatherCode == 802) return "Scattered";
    if (_weatherCode == 803) return "Broken Clouds";
    if (_weatherCode == 804) return "Overcast";
    return "Unknown";
}

void WeatherModule::render(DisplayManager& display, int x, int y, int width, int height) {
    renderHeader(display, x, y, width);
    renderRadar(display, x + RADAR_X, y + RADAR_Y, RADAR_DISPLAY_SIZE);
    renderTemperature(display, x, y);
    renderGust(display, x, y);
    renderWeatherIcons(display, x, y);
    renderGraph(display, x, y, width, height);
}

void WeatherModule::renderHeader(DisplayManager& display, int x, int y, int width) {
    // Header Text at absolute position (398, 67)
    display.drawText(398, 64, "Weather Module", 14, DisplayManager::BLACK, DisplayManager::FontType::Bold);
}

void WeatherModule::renderRadar(DisplayManager& display, int x, int y, int size) {
    if (_radarAvailable && _radarBitmap) {
        display.drawBitmap(x, y, _radarBitmap, size, size, DisplayManager::BLACK);
        // Overlay outline
        display.drawBitmap(x, y, outline_bitmap, size, size, DisplayManager::BLACK);
        // Draw border
        display.drawRect(x - 1, y - 1, size + 2, size + 2, DisplayManager::BLACK);
    } else {
        // Draw placeholder if no radar data
        display.fillRect(x, y, size, size, DisplayManager::WHITE);
        display.drawBitmap(x, y, outline_bitmap, size, size, DisplayManager::BLACK);
        display.drawRect(x - 1, y - 1, size + 2, size + 2, DisplayManager::BLACK);
    }
}

// Unused render methods
void WeatherModule::renderTemperature(DisplayManager& display, int x, int y) {
    // Temperature with 12pt font, right-aligned, moved 7px right
    String tempStr = String(_temperature);
    int16_t tempBx, tempBy; uint16_t tempBw, tempBh;
    display.getTextBounds(tempStr.c_str(), 12, &tempBx, &tempBy, &tempBw, &tempBh, DisplayManager::FontType::Bold);

    // Right-align at x=629
    int rightEdge = 629;
    int tempX = rightEdge - tempBw;  // Right-aligned: anchor at right edge
    int tempBaseline = 111;
    display.drawText(tempX, tempBaseline, tempStr.c_str(), 12, DisplayManager::BLACK, DisplayManager::FontType::Bold);

    // "c" unit text: built-in 5x7 font, moved 15px down from original baseline
    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(DisplayManager::BLACK);
    int unitX = rightEdge + 2;  // Small gap after number
    // Built-in font uses top-left positioning, moved 15px down from baseline position
    // Original: tempBaseline - 4, now: tempBaseline - 4 + 15 = tempBaseline + 11
    display.setCursor(unitX, tempBaseline + 11);
    display.print("c");
}

void WeatherModule::renderGust(DisplayManager& display, int x, int y) {
    // Gust value with 12pt font for number, smaller font for "km" unit
    // Positioned below temperature with 5px padding, moved additional 14px right
    String gustStr = String(_windGust);
    int16_t gustBx, gustBy; uint16_t gustBw, gustBh;
    display.getTextBounds(gustStr.c_str(), 12, &gustBx, &gustBy, &gustBw, &gustBh, DisplayManager::FontType::Bold);

    // Position below temperature, right-aligned, moved left 13px (636 - 13 = 623)
    int rightEdge = 623;  // 636 - 13 = 623
    int gustX = rightEdge - gustBw;  // Right-aligned
    int gustBaseline = 135;  // Temperature at y=111, 12pt height ~19px, so place gust at 111 + 19 + 5 = 135
    display.drawText(gustX, gustBaseline, gustStr.c_str(), 12, DisplayManager::BLACK, DisplayManager::FontType::Bold);

    // "km" unit text: built-in 5x7 font, moved 15px down from original baseline
    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(DisplayManager::BLACK);
    int unitX = rightEdge + 2;  // Small gap after number
    // Built-in font uses top-left positioning, moved 15px down from baseline position
    // Original: gustBaseline - 4, now: gustBaseline - 4 + 15 = gustBaseline + 11
    display.setCursor(unitX, gustBaseline + 11);
    display.print("km");
}

void WeatherModule::drawDitheredBitmap(DisplayManager& display, int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h) {
    // Draw bitmap with 50% dithering (checkerboard pattern)
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            // Check if pixel is set in bitmap
            int byteIndex = j * ((w + 7) / 8) + i / 8;
            int bitIndex = 7 - (i % 8);
            bool pixelSet = (pgm_read_byte(&bitmap[byteIndex]) & (1 << bitIndex)) != 0;

            // Draw pixel only if set AND in checkerboard pattern
            if (pixelSet && ((i + j) % 2 == 0)) {
                display.drawPixel(x + i, y + j, DisplayManager::BLACK);
            }
        }
    }
}

void WeatherModule::renderWeatherIcons(DisplayManager& display, int x, int y) {
    // Render ALL weather icons, with active one solid and others dithered
    // Icons order: Cloudy, Clear, Rainy, Snow
    // Area: 398,126 (bottom-left) to 491,110 (top-right) - moved 10px left
    int iconY = 110;
    int spacing = 5;  // Space between icons

    // Determine active weather type
    bool isClear = (_weatherCode == 800);
    bool isCloudy = (_weatherCode > 800);
    bool isRainy = (_weatherCode >= 200 && _weatherCode < 600);
    bool isFoggy = (_weatherCode >= 700 && _weatherCode < 800);
    bool isSnowy = (_weatherCode >= 600 && _weatherCode < 700);

    // Draw all 5 icons from left to right: Cloudy, Clear, Rainy, Fog, Snow
    // Cloudy at 398, Snow ends at 516 (4px right shift), redistributed spacing = 6px
    int cloudyX = 398;
    int clearX = 435;   // 398 + 31 + 6
    int rainyX = 456;   // 435 + 15 + 6
    int fogX = 478;     // 456 + 16 + 6
    int snowX = 500;    // 478 + 16 + 6 (ends at 516)

    // Cloudy icon (31x14) - offset Y by 2px since it's only 14px tall
    if (isCloudy) {
        display.drawBitmap(cloudyX, iconY + 2, icon_cloudy, 31, 14, DisplayManager::BLACK);
    } else {
        drawDitheredBitmap(display, cloudyX, iconY + 2, icon_cloudy, 31, 14);
    }

    // Clear/Sun icon (15x16)
    if (isClear) {
        display.drawBitmap(clearX, iconY, icon_clear, 15, 16, DisplayManager::BLACK);
    } else {
        drawDitheredBitmap(display, clearX, iconY, icon_clear, 15, 16);
    }

    // Rainy icon (16x16)
    if (isRainy) {
        display.drawBitmap(rainyX, iconY, icon_rainy, 16, 16, DisplayManager::BLACK);
    } else {
        drawDitheredBitmap(display, rainyX, iconY, icon_rainy, 16, 16);
    }

    // Fog icon (16x16)
    if (isFoggy) {
        display.drawBitmap(fogX, iconY, icon_fog, 16, 16, DisplayManager::BLACK);
    } else {
        drawDitheredBitmap(display, fogX, iconY, icon_fog, 16, 16);
    }

    // Snow icon (16x16) - moved 4px right
    if (isSnowy) {
        display.drawBitmap(snowX, iconY, icon_snow, 16, 16, DisplayManager::BLACK);
    } else {
        drawDitheredBitmap(display, snowX, iconY, icon_snow, 16, 16);
    }
}

void WeatherModule::renderCondition(DisplayManager& display, int x, int y) {}
void WeatherModule::renderStats(DisplayManager& display, int x, int y, int width) {}

void WeatherModule::renderGraph(DisplayManager& display, int x, int y, int width, int height) {
    // Get history data from state
    const int* tempHistory = _state->getTemperatureHistory();
    const int* gustHistory = _state->getGustHistory();
    const int* pressureHistory = _state->getPressureHistory();
    const int* rainHistory = _state->getRainHistory();
    int historyCount = _state->getHistoryCount();

    if (historyCount < 2) {
        // Not enough data to draw graphs
        return;
    }

    // TEMP: 400x216 to 517x185 (117px wide x 31px tall)
    renderSingleGraph(display, 400, 185, 117, 31,
                      tempHistory, historyCount, 0, 40, "TEMP", false);

    // GUST: 400x250 to 517x218 (117px wide x 32px tall)
    renderSingleGraph(display, 400, 218, 117, 32,
                      gustHistory, historyCount, 0, 100, "GUST", false);

    // BARO: 519x216 to 634x185 (115px wide x 31px tall)
    renderSingleGraph(display, 519, 185, 115, 31,
                      pressureHistory, historyCount, 1000, 1050, "BARO", false);

    // ACCUM: 519x250 to 634x218 (115px wide x 32px tall)
    // Rain accumulation from 3-hour data (mm)
    renderSingleGraph(display, 519, 218, 115, 32,
                      rainHistory, historyCount, 0, 30, "ACCUM", false);

    // Render 4-day forecast below graphs
    renderForecast(display, 0, 0, 0, 0);
}

void WeatherModule::renderSingleGraph(DisplayManager& display,
                                       int x, int y, int width, int height,
                                       const int* data, int count,
                                       int minVal, int maxVal,
                                       const char* label,
                                       bool drawBorder) {
    // Draw border only if requested
    if (drawBorder) {
        display.drawRect(x, y, width, height, DisplayManager::BLACK);
    }

    // Draw label (small font)
    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(DisplayManager::BLACK);
    display.setCursor(x + 2, y + 2);
    display.print(label);

    // Calculate plot area (leave margin for label)
    int plotX = x + 30;
    int plotY = y + 2;
    int plotW = width - 32;
    int plotH = height - 4;

    if (count < 2 || plotW < 10 || plotH < 10) return;

    // Calculate X step
    float xStep = (float)plotW / (count - 1);

    // Draw the trace
    int prevX = plotX;
    int prevY = plotY + plotH - 1 - map(constrain(data[0], minVal, maxVal), minVal, maxVal, 0, plotH - 1);

    for (int i = 1; i < count; i++) {
        int currX = plotX + (int)(i * xStep);
        int value = constrain(data[i], minVal, maxVal);
        int currY = plotY + plotH - 1 - map(value, minVal, maxVal, 0, plotH - 1);

        display.drawLine(prevX, prevY, currX, currY, DisplayManager::BLACK);

        prevX = currX;
        prevY = currY;
    }
}

void WeatherModule::renderForecast(DisplayManager& display, int x, int y, int width, int height) {
    // Render 4-day forecast labels starting from tomorrow
    // Using built-in font (same as graph labels)
    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(DisplayManager::BLACK);

    // Get current time to calculate tomorrow
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);

    // Day abbreviations (3 letters)
    const char* dayNames[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

    // Day label positions (baseline from left, moved 6px up)
    const int dayPositions[4][2] = {
        {401, 135},  // Day 1
        {430, 135},  // Day 2
        {459, 135},  // Day 3 (460 - 1px left)
        {488, 135}   // Day 4 (490 - 2px left)
    };

    // Temperature positions (baseline from RIGHT - right-aligned, moved 6px up and 2px right)
    const int tempPositions[4][2] = {
        {428, 147},  // Day 1 (426+2, 153-6)
        {457, 147},  // Day 2 (455+2, 153-6)
        {486, 147},  // Day 3 (484+2, 153-6)
        {515, 147}   // Day 4 (513+2, 153-6)
    };

    // Icon positions (baseline from left) - icons are 14x7 pixels
    const int iconPositions[4][2] = {
        {401, 154},  // Day 1 (moved 1px down)
        {430, 154},  // Day 2 (moved 1px down)
        {459, 154},  // Day 3 (moved 1px down)
        {488, 154}   // Day 4 (moved 1px down)
    };

    // Draw 4 day labels, icons, and temperatures starting from tomorrow
    for (int i = 0; i < 4; i++) {
        // Calculate day of week for tomorrow + i days
        time_t futureTime = now + ((i + 1) * 86400);  // Add (i+1) days in seconds
        struct tm* futureDate = localtime(&futureTime);
        int dayOfWeek = futureDate->tm_wday;

        // Draw day label in white
        display.setTextColor(DisplayManager::WHITE);
        display.setCursor(dayPositions[i][0], dayPositions[i][1]);
        display.print(dayNames[dayOfWeek]);
        display.setTextColor(DisplayManager::BLACK);  // Reset to black for temps

        // Use forecast data if available, otherwise show placeholder
        if (_hasForecast) {
            // Draw weather icon (clear=800, anything else=rain)
            const unsigned char* icon = (_forecast[i].weatherCode == 800) ? icon_forecast_clear : icon_forecast_rain;
            int iconY = iconPositions[i][1] - 7;  // Baseline - height to get top-left Y
            display.drawBitmap(iconPositions[i][0], iconY, icon, 14, 7, DisplayManager::BLACK);

            // Draw temperature (right-aligned, shift left 1px for single digit)
            String tempStr = String(_forecast[i].tempMax);
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(tempStr.c_str(), 0, 0, &tx, &ty, &tw, &th);
            int tempX = tempPositions[i][0] - tw;  // Right-align by subtracting width

            // If single digit (< 10), shift left 1 pixel
            if (_forecast[i].tempMax < 10) {
                tempX -= 1;
            }

            display.setCursor(tempX, tempPositions[i][1]);
            display.print(tempStr);
        }
    }
}

void WeatherModule::renderTimestamp(DisplayManager& display, int x, int y, bool isStale) {}