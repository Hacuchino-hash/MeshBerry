/**
 * MeshBerry Status Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Device status and information screen
 */

#ifndef MESHBERRY_STATUSSCREEN_H
#define MESHBERRY_STATUSSCREEN_H

#include "Screen.h"
#include "ScreenManager.h"

class StatusScreen : public Screen {
public:
    StatusScreen() = default;
    ~StatusScreen() override = default;

    ScreenId getId() const override { return ScreenId::STATUS; }
    void onEnter() override;
    void onExit() override {}
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    const char* getTitle() const override { return "Status"; }
    void configureSoftKeys() override;

    // Update device status data
    void setBatteryInfo(uint8_t percent, uint16_t millivolts);
    void setRadioInfo(float freq, int sf, float bw, int txPower);
    void setMeshInfo(int nodeCount, int msgCount, bool forwarding);
    void setGpsInfo(bool present, bool hasFix = false);

private:
    // Device info
    uint8_t _batteryPercent = 0;
    uint16_t _batteryMv = 0;
    float _loraFreq = 915.0f;
    int _loraSf = 10;
    float _loraBw = 250.0f;
    int _loraTxPower = 22;
    int _nodeCount = 0;
    int _msgCount = 0;
    bool _forwarding = false;
    bool _gpsPresent = false;
    bool _gpsFix = false;
};

#endif // MESHBERRY_STATUSSCREEN_H
