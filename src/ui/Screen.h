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
    SOFTKEY_RIGHT    // Right soft key (ESC/Backspace)
};

/**
 * Input data structure
 */
struct InputData {
    InputEvent event;
    uint8_t keyCode;    // Raw key code for KEY_PRESS
    char keyChar;       // ASCII character for KEY_PRESS (0 for special keys)

    InputData() : event(InputEvent::NONE), keyCode(0), keyChar(0) {}
    InputData(InputEvent e) : event(e), keyCode(0), keyChar(0) {}
    InputData(InputEvent e, uint8_t code, char c) : event(e), keyCode(code), keyChar(c) {}
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
    CONTACTS,
    CONTACT_DETAIL,
    REPEATER_ADMIN, // Repeater administration
    REPEATER_CLI,   // Repeater CLI terminal
    SETTINGS,
    SETTINGS_RADIO,
    SETTINGS_DISPLAY,
    SETTINGS_NETWORK,
    STATUS,
    CHANNELS,
    GPS,
    ABOUT
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
