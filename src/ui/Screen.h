/**
 * MeshBerry Screen Base Class
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Abstract base class for all screens in the UI
 */

#ifndef MESHBERRY_SCREEN_H
#define MESHBERRY_SCREEN_H

#include <Arduino.h>
#include "Theme.h"

// Forward declaration
class ScreenManager;

/**
 * Input event types
 */
enum class InputEvent {
    NONE,
    KEY_PRESS,       // Keyboard key pressed
    TRACKBALL_UP,
    TRACKBALL_DOWN,
    TRACKBALL_LEFT,
    TRACKBALL_RIGHT,
    TRACKBALL_CLICK,
    BACK,            // ESC or Back button
    MENU,            // Menu key
    SOFTKEY_LEFT,    // Left soft key (Q key)
    SOFTKEY_CENTER,  // Center soft key (Enter/Space)
    SOFTKEY_RIGHT,   // Right soft key (ESC/Backspace)
    // Touch events
    TOUCH_TAP,       // Single tap at coordinates
    TOUCH_DRAG,      // Continuous touch with position delta (for scrolling)
    TOUCH_RELEASE    // Touch released
};

/**
 * Input data structure
 */
struct InputData {
    InputEvent event;
    uint8_t keyCode;    // Raw key code for KEY_PRESS
    char keyChar;       // ASCII character for KEY_PRESS (0 for special keys)
    int16_t touchX;     // Touch X coordinate (0-319) for TOUCH_TAP/TOUCH_DRAG
    int16_t touchY;     // Touch Y coordinate (0-239) for TOUCH_TAP/TOUCH_DRAG
    int16_t dragDeltaY; // Vertical drag delta for TOUCH_DRAG (scrolling)

    InputData() : event(InputEvent::NONE), keyCode(0), keyChar(0), touchX(0), touchY(0), dragDeltaY(0) {}
    InputData(InputEvent e) : event(e), keyCode(0), keyChar(0), touchX(0), touchY(0), dragDeltaY(0) {}
    InputData(InputEvent e, uint8_t code, char c) : event(e), keyCode(code), keyChar(c), touchX(0), touchY(0), dragDeltaY(0) {}
    InputData(InputEvent e, int16_t x, int16_t y) : event(e), keyCode(0), keyChar(0), touchX(x), touchY(y), dragDeltaY(0) {}
    // Constructor for TOUCH_DRAG with delta
    InputData(InputEvent e, int16_t x, int16_t y, int16_t dy) : event(e), keyCode(0), keyChar(0), touchX(x), touchY(y), dragDeltaY(dy) {}
};

/**
 * Screen IDs for navigation
 */
enum class ScreenId {
    NONE,
    HOME,
    MESSAGES,
    CHAT,           // Channel chat view
    DM_CHAT,        // Direct message chat view
    DM_SETTINGS,    // DM routing settings for a contact
    CONTACTS,
    CONTACT_DETAIL,
    REPEATER_ADMIN, // Repeater administration
    REPEATER_CLI,   // Repeater CLI terminal
    SETTINGS,
    SETTINGS_RADIO,
    SETTINGS_DISPLAY,
    SETTINGS_NETWORK,
    SETTINGS_WIFI,  // WiFi configuration screen
    STATUS,
    CHANNELS,
    GPS,
    ABOUT,
    EMOJI_PICKER    // Emoji selection screen
};

/**
 * Abstract Screen base class
 */
class Screen {
public:
    virtual ~Screen() = default;

    /**
     * Get the screen ID
     */
    virtual ScreenId getId() const = 0;

    /**
     * Called when screen becomes active
     * Use to initialize/reset state
     */
    virtual void onEnter() {}

    /**
     * Called when screen is about to be deactivated
     */
    virtual void onExit() {}

    /**
     * Draw the screen content
     * @param fullRedraw True if entire screen should be redrawn
     */
    virtual void draw(bool fullRedraw = false) = 0;

    /**
     * Handle input event
     * @param input The input event
     * @return True if the event was consumed
     */
    virtual bool handleInput(const InputData& input) = 0;

    /**
     * Called periodically for animations/updates
     * @param deltaMs Milliseconds since last update
     */
    virtual void update(uint32_t deltaMs) {}

    /**
     * Get the title for the status bar (optional)
     */
    virtual const char* getTitle() const { return nullptr; }

    /**
     * Configure soft key labels for this screen
     */
    virtual void configureSoftKeys() {}

    /**
     * Check if screen needs redraw
     */
    virtual bool needsRedraw() const { return _needsRedraw; }

    /**
     * Request a redraw
     */
    void requestRedraw() { _needsRedraw = true; }

    /**
     * Clear redraw flag
     */
    void clearRedrawFlag() { _needsRedraw = false; }

protected:
    bool _needsRedraw = true;
};

#endif // MESHBERRY_SCREEN_H
