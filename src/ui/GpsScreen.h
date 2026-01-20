/**
 * MeshBerry GPS Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Displays GPS location information from u-blox MIA-M10Q module
 */

#ifndef MESHBERRY_GPSSCREEN_H
#define MESHBERRY_GPSSCREEN_H

#include "Screen.h"
#include "ScreenManager.h"

class GpsScreen : public Screen {
public:
    GpsScreen() = default;
    ~GpsScreen() override = default;

    ScreenId getId() const override { return ScreenId::GPS; }
    void onEnter() override;
    void onExit() override {}
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    const char* getTitle() const override { return "GPS"; }
    void configureSoftKeys() override;

private:
    // Cached display values to detect changes for partial redraw
    double _lastLat = 0;
    double _lastLon = 0;
    double _lastAlt = 0;
    float _lastSpeed = 0;
    float _lastCourse = 0;
    uint8_t _lastSats = 0;
    float _lastHdop = 0;
    bool _lastFix = false;
    uint32_t _lastUpdate = 0;

    // Draw helper functions
    void drawDataRow(int16_t y, const char* label, const char* value,
                     uint16_t labelColor, uint16_t valueColor);
    void drawNoGps();
    void drawAcquiring();
    void drawGpsData();

    // Format helpers
    void formatLatitude(double lat, char* buf, size_t bufSize);
    void formatLongitude(double lon, char* buf, size_t bufSize);
    void formatFixAge(uint32_t ageMs, char* buf, size_t bufSize);
    const char* getHdopQuality(float hdop);
};

#endif // MESHBERRY_GPSSCREEN_H
