/**
 * MeshBerry UI Component - Button
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Reusable button component with states and callbacks
 */

#ifndef MESHBERRY_BUTTON_H
#define MESHBERRY_BUTTON_H

#include <Arduino.h>
#include <functional>
#include "../../drivers/display.h"

/**
 * Button component with visual states and click handling
 */
class Button {
private:
    int16_t _x, _y, _w, _h;
    String _label;
    const uint8_t* _icon;  // Optional icon (16x16 bitmap)
    bool _isPrimary;
    Display::ButtonState _state;
    std::function<void()> _onClick;
    bool _enabled;

public:
    /**
     * Create a button
     * @param x X position
     * @param y Y position
     * @param w Width
     * @param label Button label text
     */
    Button(int16_t x, int16_t y, int16_t w, const char* label);

    /**
     * Set button icon (optional, 16x16 bitmap)
     */
    void setIcon(const uint8_t* icon);

    /**
     * Set button as primary (filled) or secondary (outline)
     */
    void setPrimary(bool isPrimary);

    /**
     * Set button state (normal, hover, pressed, disabled)
     */
    void setState(Display::ButtonState state);

    /**
     * Set enabled/disabled
     */
    void setEnabled(bool enabled);

    /**
     * Set click callback
     */
    void setOnClick(std::function<void()> callback);

    /**
     * Set label text
     */
    void setLabel(const char* label);

    /**
     * Set position
     */
    void setPosition(int16_t x, int16_t y);

    /**
     * Set width
     */
    void setWidth(int16_t w);

    /**
     * Draw the button
     */
    void draw();

    /**
     * Handle input - check if click occurred at position
     * @param x Touch/click X coordinate
     * @param y Touch/click Y coordinate
     * @return true if button was clicked
     */
    bool handleClick(int16_t x, int16_t y);

    /**
     * Get button bounds
     */
    void getBounds(int16_t& x, int16_t& y, int16_t& w, int16_t& h);

    /**
     * Check if point is within button
     */
    bool contains(int16_t x, int16_t y);
};

#endif // MESHBERRY_BUTTON_H
