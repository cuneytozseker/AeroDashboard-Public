#include "DebugServer.h"
#include "DisplayManager.h"
#include <SharedConfig.h>
#include <WiFi.h>

DebugServer::DebugServer(uint16_t port)
    : _server(port), _display(nullptr), _running(false)
{}

void DebugServer::begin(DisplayManager* display) {
    _display = display;

    // Register HTTP handlers
    _server.on("/", [this]() { handleRoot(); });
    _server.on("/screenshot", [this]() { handleScreenshot(); });
    _server.on("/logs", [this]() { handleLogs(); });
    _server.onNotFound([this]() { handleNotFound(); });

    _server.begin();
    _running = true;

    DEBUG_PRINTLN("Debug server started on port 80");
    DEBUG_PRINT("Access at: http://");
    DEBUG_PRINT(WiFi.localIP());
    DEBUG_PRINTLN("/");
}

void DebugServer::handleClient() {
    if (_running) {
        _server.handleClient();
    }
}

void DebugServer::handleRoot() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>AeroDashboard Debug</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 900px;
            margin: 20px auto;
            padding: 20px;
            background: #f0f0f0;
        }
        h1 {
            color: #333;
        }
        .controls {
            margin: 20px 0;
        }
        button {
            padding: 10px 20px;
            font-size: 16px;
            cursor: pointer;
            background: #007bff;
            color: white;
            border: none;
            border-radius: 4px;
        }
        button:hover {
            background: #0056b3;
        }
        #display {
            border: 2px solid #333;
            background: white;
            max-width: 100%;
            height: auto;
        }
        .info {
            margin: 10px 0;
            padding: 10px;
            background: white;
            border-radius: 4px;
        }
    </style>
</head>
<body>
    <h1>AeroDashboard Debug View</h1>
    <div class="info">
        <strong>Display:</strong> 792x272 pixels (5.79" E-Paper)<br>
        <strong>Last Update:</strong> <span id="timestamp">Never</span>
    </div>
    <div class="controls">
        <button onclick="refreshScreen()">Refresh Screenshot</button>
        <button onclick="toggleAutoRefresh()">Auto-Refresh: <span id="autoStatus">OFF</span></button>
        <a href="/logs"><button type="button">View Logs</button></a>
    </div>
    <img id="display" src="/screenshot" alt="Display Screenshot">

    <script>
        let autoRefresh = false;
        let autoRefreshInterval = null;

        function refreshScreen() {
            const img = document.getElementById('display');
            img.src = '/screenshot?' + new Date().getTime();
            document.getElementById('timestamp').textContent = new Date().toLocaleTimeString();
        }

        function toggleAutoRefresh() {
            autoRefresh = !autoRefresh;
            const status = document.getElementById('autoStatus');

            if (autoRefresh) {
                status.textContent = 'ON';
                autoRefreshInterval = setInterval(refreshScreen, 2000);
            } else {
                status.textContent = 'OFF';
                if (autoRefreshInterval) {
                    clearInterval(autoRefreshInterval);
                }
            }
        }
    </script>
</body>
</html>
)rawliteral";

    _server.send(200, "text/html", html);
}

void DebugServer::handleScreenshot() {
    if (!_display) {
        _server.send(500, "text/plain", "Display manager not initialized");
        return;
    }

    sendBMPImage();
}

void DebugServer::handleLogs() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>AeroDashboard Logs</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: monospace; background: #111; color: #0f0; margin: 0; padding: 16px; }
        h1 { color: #0f0; font-size: 16px; margin-bottom: 8px; }
        .toolbar { margin-bottom: 10px; }
        button { padding: 6px 14px; font-size: 13px; cursor: pointer; background: #333; color: #0f0;
                 border: 1px solid #0f0; border-radius: 3px; margin-right: 6px; }
        button:hover { background: #0f0; color: #111; }
        #log { white-space: pre-wrap; font-size: 13px; line-height: 1.5;
               background: #000; padding: 12px; border: 1px solid #333;
               border-radius: 4px; min-height: 200px; }
        a { color: #0f0; }
    </style>
</head>
<body>
    <h1>AeroDashboard / Logs</h1>
    <div class="toolbar">
        <button onclick="fetchLogs()">Refresh</button>
        <button onclick="toggleAuto()">Auto: <span id="autoLbl">OFF</span></button>
        <a href="/"><button type="button">Back</button></a>
    </div>
    <div id="log">Loading...</div>
    <script>
        let timer = null;
        function fetchLogs() {
            fetch('/logs?raw=1').then(r => r.text()).then(t => {
                document.getElementById('log').textContent = t;
            });
        }
        function toggleAuto() {
            if (timer) { clearInterval(timer); timer = null; document.getElementById('autoLbl').textContent = 'OFF'; }
            else { timer = setInterval(fetchLogs, 2000); document.getElementById('autoLbl').textContent = 'ON'; }
        }
        fetchLogs();
    </script>
</body>
</html>
)rawliteral";

    // ?raw=1 returns plain text for the JS fetch
    if (_server.hasArg("raw")) {
        String out;
        out.reserve(LOG_BUFFER_LINES * LOG_LINE_MAX_LEN);
        int total = gLogBuffer.count;
        int start = (gLogBuffer.head - total + LOG_BUFFER_LINES) % LOG_BUFFER_LINES;
        for (int i = 0; i < total; i++) {
            int idx = (start + i) % LOG_BUFFER_LINES;
            out += String(i + 1);
            out += ": ";
            out += gLogBuffer.lines[idx];
            out += "\n";
        }
        if (total == 0) out = "(no log entries yet)";
        _server.send(200, "text/plain", out);
    } else {
        _server.send(200, "text/html", html);
    }
}

void DebugServer::handleNotFound() {
    _server.send(404, "text/plain", "Not Found");
}

void DebugServer::sendBMPImage() {
    // BMP file format for monochrome 792x272 image
    const int width = DISPLAY_WIDTH;
    const int height = DISPLAY_HEIGHT;
    const int rowSize = ((width + 31) / 32) * 4;  // Row size must be multiple of 4 bytes
    const int imageSize = rowSize * height;
    const int fileSize = 62 + imageSize;  // 62 bytes header + image data

    // Send HTTP headers
    _server.setContentLength(fileSize);
    _server.send(200, "image/bmp", "");

    WiFiClient client = _server.client();

    // BMP file header (14 bytes)
    uint8_t bmpFileHeader[14] = {
        'B', 'M',                    // Signature
        fileSize & 0xFF, (fileSize >> 8) & 0xFF, (fileSize >> 16) & 0xFF, (fileSize >> 24) & 0xFF,  // File size
        0, 0, 0, 0,                  // Reserved
        62, 0, 0, 0                  // Pixel data offset
    };
    client.write(bmpFileHeader, 14);

    // BMP info header (40 bytes)
    uint8_t bmpInfoHeader[40] = {
        40, 0, 0, 0,                 // Header size
        width & 0xFF, (width >> 8) & 0xFF, (width >> 16) & 0xFF, (width >> 24) & 0xFF,  // Width
        height & 0xFF, (height >> 8) & 0xFF, (height >> 16) & 0xFF, (height >> 24) & 0xFF,  // Height
        1, 0,                        // Color planes
        1, 0,                        // Bits per pixel (1-bit monochrome)
        0, 0, 0, 0,                  // Compression (none)
        imageSize & 0xFF, (imageSize >> 8) & 0xFF, (imageSize >> 16) & 0xFF, (imageSize >> 24) & 0xFF,  // Image size
        0, 0, 0, 0,                  // X pixels per meter
        0, 0, 0, 0,                  // Y pixels per meter
        2, 0, 0, 0,                  // Colors in palette
        0, 0, 0, 0                   // Important colors
    };
    client.write(bmpInfoHeader, 40);

    // Color palette (8 bytes: white, black)
    uint8_t palette[8] = {
        255, 255, 255, 0,  // White
        0, 0, 0, 0         // Black
    };
    client.write(palette, 8);

    // Get shadow buffer from display manager
    const uint8_t* buffer = _display->getBuffer();

    if (!buffer) {
        client.println("Error: Shadow buffer not available");
        return;
    }

    // Write pixel data (bottom-up, BMP format)
    uint8_t rowBuffer[rowSize];
    for (int y = height - 1; y >= 0; y--) {
        memset(rowBuffer, 0, rowSize);

        // Copy row data (1 bit per pixel, packed into bytes)
        int bufferRowOffset = y * ((width + 7) / 8);
        int bytesPerRow = (width + 7) / 8;

        for (int x = 0; x < bytesPerRow; x++) {
            // Invert bits (GxEPD2: 1=white, BMP: 0=white)
            rowBuffer[x] = ~buffer[bufferRowOffset + x];
        }

        client.write(rowBuffer, rowSize);
    }
}
