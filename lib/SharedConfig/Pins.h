#ifndef PINS_H
#define PINS_H

// =============================================================================
// AeroDashboard Pin Definitions
// =============================================================================
// Elecrow CrowPanel ESP32-S3 5.79" E-Paper Display
// Display Controller: Dual SSD1683 (handled by GxEPD2_579_GDEY0579T93)
// =============================================================================

// -----------------------------------------------------------------------------
// E-Paper Display SPI Pins
// -----------------------------------------------------------------------------
// These pins are typically fixed for the CrowPanel - verify with your hardware
// The GxEPD2 library will use these for communication with the display

#define EPD_CS          45      // Chip Select
#define EPD_DC          46      // Data/Command
#define EPD_RST         47      // Reset
#define EPD_BUSY        48      // Busy signal

// SPI Bus pins (ESP32-S3 default HSPI)
#define EPD_SCK         12      // SPI Clock
#define EPD_MOSI        11      // SPI MOSI (Master Out Slave In)
// Note: MISO not used for e-paper (write-only)

// -----------------------------------------------------------------------------
// Alternative Pin Configuration
// -----------------------------------------------------------------------------
// If your CrowPanel uses different pins, uncomment and modify these:

// #define EPD_CS          5
// #define EPD_DC          17
// #define EPD_RST         16
// #define EPD_BUSY        4
// #define EPD_SCK         18
// #define EPD_MOSI        23

// -----------------------------------------------------------------------------
// Power Control (CrowPanel e-paper power enable)
// -----------------------------------------------------------------------------
// The CrowPanel uses pin 7 to control e-paper display power via MOSFET
#define EPD_POWER_PIN       7       // Display power control
#define EPD_POWER_ON        HIGH    // State to turn display ON
#define EPD_POWER_OFF       LOW     // State to turn display OFF

// -----------------------------------------------------------------------------
// Status LED (optional)
// -----------------------------------------------------------------------------
// Built-in LED for status indication during boot/fetch

#define LED_BUILTIN_PIN     2       // Most ESP32-S3 boards
// #define LED_BUILTIN_PIN  48      // Some ESP32-S3 variants

// -----------------------------------------------------------------------------
// Button Input (optional)
// -----------------------------------------------------------------------------
// For manual refresh trigger if hardware supports it

// #define BUTTON_PIN      0       // Boot button on most ESP32 boards
// #define BUTTON_ACTIVE   LOW     // Button pressed state

// -----------------------------------------------------------------------------
// I2C Pins (for future expansion - RTC, sensors, etc.)
// -----------------------------------------------------------------------------
#define I2C_SDA         21
#define I2C_SCL         22

// -----------------------------------------------------------------------------
// Display Dimensions (hardware constants)
// -----------------------------------------------------------------------------
#define EPD_WIDTH       792
#define EPD_HEIGHT      272

#endif // PINS_H
