/**
 * MeshBerry GPS Driver
 *
 * Interface for u-blox MIA-M10Q GNSS module (T-Deck Plus only)
 *
 * Hardware: u-blox MIA-M10Q
 * Interface: UART (9600 baud)
 * Protocol: NMEA
 *
 * Note: GPS is optional - only present on T-Deck Plus model
 *       Use GPS::isPresent() to check availability at runtime
 *
 * See: dev-docs/REQUIREMENTS.md for specifications
 */

#ifndef MESHBERRY_GPS_H
#define MESHBERRY_GPS_H

#include <Arduino.h>
#include "../config.h"

// =============================================================================
// GPS DATA STRUCTURES
// =============================================================================

typedef struct {
    double      latitude;
    double      longitude;
    double      altitude;       // Meters above sea level
    float       speed;          // km/h
    float       course;         // Degrees
    uint8_t     satellites;     // Number of satellites in view
    float       hdop;           // Horizontal dilution of precision
    bool        valid;          // Fix is valid
    uint32_t    timestamp;      // UTC time (HHMMSS * 100 + milliseconds)
    uint32_t    date;           // Date (DDMMYY)
} GPSData_t;

// =============================================================================
// GPS DRIVER INTERFACE
// =============================================================================

namespace GPS {

/**
 * Initialize the GPS hardware
 * @return true if GPS module is detected
 */
bool init();

/**
 * Check if GPS module is present
 * (T-Deck Plus has GPS, base T-Deck does not)
 */
bool isPresent();

/**
 * Update GPS data from serial buffer
 * Call this regularly in the main loop
 */
void update();

/**
 * Check if we have a valid GPS fix
 */
bool hasFix();

/**
 * Get current GPS data
 */
GPSData_t getData();

/**
 * Get latitude
 */
double getLatitude();

/**
 * Get longitude
 */
double getLongitude();

/**
 * Get altitude in meters
 */
double getAltitude();

/**
 * Get speed in km/h
 */
float getSpeed();

/**
 * Get course in degrees
 */
float getCourse();

/**
 * Get number of satellites
 */
uint8_t getSatellites();

/**
 * Get horizontal dilution of precision
 * Lower is better (<1 = ideal, 1-2 = excellent, 2-5 = good)
 */
float getHDOP();

/**
 * Get age of last valid fix in milliseconds
 */
uint32_t getFixAge();

/**
 * Get distance to another point
 * @param lat Destination latitude
 * @param lng Destination longitude
 * @return Distance in meters
 */
double distanceTo(double lat, double lng);

/**
 * Get bearing to another point
 * @param lat Destination latitude
 * @param lng Destination longitude
 * @return Bearing in degrees (0-360)
 */
double bearingTo(double lat, double lng);

/**
 * Enable/disable GPS
 * Turning off saves power on T-Deck Plus
 */
void enable();
void disable();
bool isEnabled();

/**
 * Put GPS in low power mode
 * Reduces update rate to save power
 */
void setPowerSave(bool enable);

} // namespace GPS

#endif // MESHBERRY_GPS_H
