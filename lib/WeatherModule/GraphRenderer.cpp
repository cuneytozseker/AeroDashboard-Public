#include "GraphRenderer.h"
#include "DisplayManager.h"

// Graph configuration
#define GRAPH_MARGIN_LEFT   25
#define GRAPH_MARGIN_RIGHT  5
#define GRAPH_MARGIN_TOP    5
#define GRAPH_MARGIN_BOTTOM 12

// Data ranges
#define TEMP_MIN    -10
#define TEMP_MAX    45
#define PRESSURE_MIN 980
#define PRESSURE_MAX 1040
#define GUST_MIN    0
#define GUST_MAX    100

// =============================================================================
// Constructor
// =============================================================================

GraphRenderer::GraphRenderer() {
}

// =============================================================================
// Main Render Function
// =============================================================================

void GraphRenderer::render(DisplayManager& display,
                            int x, int y, int width, int height,
                            const int* tempHistory,
                            const int* pressureHistory,
                            const int* gustHistory,
                            int count) {
    // Calculate plot area
    int plotX = x + GRAPH_MARGIN_LEFT;
    int plotY = y + GRAPH_MARGIN_TOP;
    int plotW = width - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
    int plotH = height - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;

    // Draw frame and labels
    drawFrame(display, x, y, width, height);

    if (count < 2) {
        // Not enough data to draw
        return;
    }

    // Draw temperature trace (solid line)
    drawTrace(display, plotX, plotY, plotW, plotH,
              tempHistory, count, TEMP_MIN, TEMP_MAX, false);

    // Draw pressure trace (dashed line) - normalized to fit in same range
    // Create normalized array
    int normalizedPressure[GRAPH_HISTORY_POINTS];
    for (int i = 0; i < count; i++) {
        // Map pressure (980-1040) to temperature range (-10 to 45)
        normalizedPressure[i] = map(pressureHistory[i],
                                    PRESSURE_MIN, PRESSURE_MAX,
                                    TEMP_MIN, TEMP_MAX);
    }
    drawTrace(display, plotX, plotY, plotW, plotH,
              normalizedPressure, count, TEMP_MIN, TEMP_MAX, true);

    // Note: Wind gust could be added as a third trace with different pattern
    // For now, keeping graph simple with two traces
}

// =============================================================================
// Frame Drawing
// =============================================================================

void GraphRenderer::drawFrame(DisplayManager& display, int x, int y, int width, int height) {
    int plotX = x + GRAPH_MARGIN_LEFT;
    int plotY = y + GRAPH_MARGIN_TOP;
    int plotW = width - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
    int plotH = height - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;

    // Draw border
    display.drawRect(plotX, plotY, plotW, plotH, DisplayManager::BLACK);

    // Draw horizontal grid lines (3 lines dividing into 4 sections)
    for (int i = 1; i < 4; i++) {
        int lineY = plotY + (plotH * i) / 4;
        // Dotted line
        for (int dx = 0; dx < plotW; dx += 4) {
            display.drawPixel(plotX + dx, lineY, DisplayManager::BLACK);
        }
    }

    // Y-axis labels (temperature scale)
    display.setFont(nullptr); // Small built-in font

    // Top label
    display.setCursor(x, plotY + 5);
    display.print(TEMP_MAX);

    // Middle label
    display.setCursor(x, plotY + plotH/2);
    display.print((TEMP_MAX + TEMP_MIN) / 2);

    // Bottom label
    display.setCursor(x, plotY + plotH - 2);
    display.print(TEMP_MIN);

    // Time axis label
    display.setCursor(plotX, y + height - 2);
    display.print("-6h");
    display.setCursor(plotX + plotW - 15, y + height - 2);
    display.print("now");
}

// =============================================================================
// Trace Drawing
// =============================================================================

void GraphRenderer::drawTrace(DisplayManager& display,
                               int x, int y, int width, int height,
                               const int* data, int count,
                               int minVal, int maxVal,
                               bool dashed) {
    if (count < 2) return;

    // Calculate X step (spread points across width)
    float xStep = (float)width / (count - 1);

    int prevX = x;
    int prevY = mapToY(data[0], minVal, maxVal, y, height);

    for (int i = 1; i < count; i++) {
        int currX = x + (int)(i * xStep);
        int currY = mapToY(data[i], minVal, maxVal, y, height);

        if (dashed) {
            // Draw dashed line
            int dx = currX - prevX;
            int dy = currY - prevY;
            float lineLength = sqrt(dx*dx + dy*dy);
            int segments = (int)(lineLength / 4); // 4-pixel segments

            if (segments > 0) {
                float segX = (float)dx / segments;
                float segY = (float)dy / segments;

                for (int s = 0; s < segments; s += 2) {
                    int x1 = prevX + (int)(s * segX);
                    int y1 = prevY + (int)(s * segY);
                    int x2 = prevX + (int)((s + 1) * segX);
                    int y2 = prevY + (int)((s + 1) * segY);
                    display.drawLine(x1, y1, x2, y2, DisplayManager::BLACK);
                }
            }
        } else {
            // Draw solid line
            display.drawLine(prevX, prevY, currX, currY, DisplayManager::BLACK);
        }

        prevX = currX;
        prevY = currY;
    }
}

// =============================================================================
// Helper Functions
// =============================================================================

int GraphRenderer::mapToY(int value, int minVal, int maxVal, int y, int height) {
    // Clamp value to range
    if (value < minVal) value = minVal;
    if (value > maxVal) value = maxVal;

    // Map value to Y (inverted because Y increases downward)
    return y + height - 1 - map(value, minVal, maxVal, 0, height - 1);
}

void GraphRenderer::findMinMax(const int* data, int count, int* minVal, int* maxVal) {
    if (count == 0) {
        *minVal = 0;
        *maxVal = 100;
        return;
    }

    *minVal = data[0];
    *maxVal = data[0];

    for (int i = 1; i < count; i++) {
        if (data[i] < *minVal) *minVal = data[i];
        if (data[i] > *maxVal) *maxVal = data[i];
    }

    // Add some padding
    int range = *maxVal - *minVal;
    if (range < 10) range = 10;
    *minVal -= range / 10;
    *maxVal += range / 10;
}
