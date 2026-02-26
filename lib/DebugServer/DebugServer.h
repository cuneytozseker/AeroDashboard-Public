#ifndef DEBUG_SERVER_H
#define DEBUG_SERVER_H

#include <Arduino.h>
#include <WebServer.h>

// Forward declaration
class DisplayManager;

/**
 * DebugServer - HTTP server for development feedback
 *
 * Provides a web interface to view the current display framebuffer
 * in real-time without needing to physically check the e-paper display.
 */
class DebugServer {
public:
    DebugServer(uint16_t port = 80);

    // Initialize with reference to display manager
    void begin(DisplayManager* display);

    // Process incoming requests (call in loop)
    void handleClient();

    // Check if server is running
    bool isRunning() const { return _running; }

private:
    // HTTP request handlers
    void handleRoot();
    void handleScreenshot();
    void handleLogs();
    void handleNotFound();

    // Generate BMP image from framebuffer
    void sendBMPImage();

    WebServer _server;
    DisplayManager* _display;
    bool _running;
};

#endif // DEBUG_SERVER_H
