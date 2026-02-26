#ifndef GRAPH_RENDERER_H
#define GRAPH_RENDERER_H

#include <Arduino.h>
#include <SharedConfig.h>

// Forward declaration
class DisplayManager;

/**
 * GraphRenderer - Renders multi-trace history graphs
 *
 * Displays temperature, pressure, and wind gust history
 * over a 6-hour period with labeled axes.
 */
class GraphRenderer {
public:
    GraphRenderer();

    /**
     * Render the history graph
     * @param display Reference to display manager
     * @param x, y Top-left corner position
     * @param width, height Graph dimensions
     * @param tempHistory Temperature data points (Celsius)
     * @param pressureHistory Pressure data points (hPa)
     * @param gustHistory Wind gust data points (km/h)
     * @param count Number of valid data points
     */
    void render(DisplayManager& display,
                int x, int y, int width, int height,
                const int* tempHistory,
                const int* pressureHistory,
                const int* gustHistory,
                int count);

private:
    // Draw graph frame and labels
    void drawFrame(DisplayManager& display, int x, int y, int width, int height);

    // Draw a single trace
    void drawTrace(DisplayManager& display,
                   int x, int y, int width, int height,
                   const int* data, int count,
                   int minVal, int maxVal,
                   bool dashed = false);

    // Map value to Y coordinate
    int mapToY(int value, int minVal, int maxVal, int y, int height);

    // Find min/max in data
    void findMinMax(const int* data, int count, int* minVal, int* maxVal);
};

#endif // GRAPH_RENDERER_H
