/**
 * MeshBerry UI Component - ToggleSwitch
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * iOS-style toggle switch for boolean settings
 */

#ifndef MESHBERRY_TOGGLESWITCH_H
#define MESHBERRY_TOGGLESWITCH_H

#include <Arduino.h>
#include <functional>

/**
 * Toggle switch component (ON/OFF)
 */
class ToggleSwitch {
private:
    int16_t _x, _y;
    bool _isOn;
    bool _isEnabled;
    std::function<void(bool)> _onChange;

    // Toggle dimensions (iOS-style)
    static constexpr int16_t TOGGLE_W = 48;
    static constexpr int16_t TOGGLE_H = 28;

public:
    /**
     * Create a toggle switch
     * @param x X position (left edge)
     * @param y Y position (top edge)
     * @param initialState Initial ON/OFF state
     */
    ToggleSwitch(int16_t x, int16_t y, bool initialState = false);

    /**
     * Set enabled/disabled state
     */
    void setEnabled(bool enabled);

    /**
     * Set ON/OFF state
     */
    void setState(bool isOn);

    /**
     * Set change callback (called when toggled)
     * @param callback Function called with new state
     */
    void setOnChange(std::function<void(bool)> callback);

    /**
     * Set position
     */
    void setPosition(int16_t x, int16_t y);

    /**
     * Draw the toggle switch
     */
    void draw();

    /**
     * Handle click at position
     * @param x Click X coordinate
     * @param y Click Y coordinate
     * @return true if toggle was clicked and changed state
     */
    bool handleClick(int16_t x, int16_t y);

    /**
     * Get current state
     */
    bool isOn() const { return _isOn; }

    /**
     * Get enabled state
     */
    bool isEnabled() const { return _isEnabled; }

    /**
     * Get toggle bounds
     */
    void getBounds(int16_t& x, int16_t& y, int16_t& w, int16_t& h);

    /**
     * Check if point is within toggle
     */
    bool contains(int16_t x, int16_t y);

    /**
     * Get standard toggle width (for layout calculations)
     */
    static int16_t getWidth() { return TOGGLE_W; }

    /**
     * Get standard toggle height (for layout calculations)
     */
    static int16_t getHeight() { return TOGGLE_H; }
};

#endif // MESHBERRY_TOGGLESWITCH_H
