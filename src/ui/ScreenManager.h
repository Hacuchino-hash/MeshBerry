/**
 * MeshBerry Screen Manager
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Manages screen navigation stack and input routing
 */

#ifndef MESHBERRY_SCREENMANAGER_H
#define MESHBERRY_SCREENMANAGER_H

#include <Arduino.h>
#include "Screen.h"
#include "StatusBar.h"
#include "SoftKeyBar.h"

/**
 * Maximum depth of screen navigation stack
 */
constexpr int MAX_SCREEN_STACK = 8;

/**
 * Screen Manager singleton
 */
class ScreenManager {
public:
    /**
     * Get the singleton instance
     */
    static ScreenManager& instance();

    /**
     * Initialize the screen manager
     */
    void init();

    /**
     * Register a screen instance
     * @param screen Pointer to screen (manager does NOT take ownership)
     */
    void registerScreen(Screen* screen);

    /**
     * Navigate to a screen by ID
     * @param id Screen ID to navigate to
     * @param pushToStack If true, current screen is pushed to back stack
     */
    void navigateTo(ScreenId id, bool pushToStack = true);

    /**
     * Go back to previous screen
     * @return True if there was a screen to go back to
     */
    bool goBack();

    /**
     * Go to home screen, clearing the stack
     */
    void goHome();

    /**
     * Get current active screen
     */
    Screen* getCurrentScreen() const { return _currentScreen; }

    /**
     * Get current screen ID
     */
    ScreenId getCurrentScreenId() const;

    /**
     * Handle input event (call from main loop)
     * @param input The input event
     */
    void handleInput(const InputData& input);

    /**
     * Process keyboard input (convenience method)
     * @param keyCode Raw key code
     * @param keyChar ASCII character (0 for special keys)
     */
    void handleKey(uint8_t keyCode, char keyChar);

    /**
     * Process trackball input (convenience method)
     * @param up Up movement
     * @param down Down movement
     * @param left Left movement
     * @param right Right movement
     * @param click Click/select
     */
    void handleTrackball(bool up, bool down, bool left, bool right, bool click);

    /**
     * Main loop update (call regularly)
     * Updates screens, status bar, and redraws as needed
     */
    void update();

    /**
     * Force full redraw of everything
     */
    void forceRedraw();

    /**
     * Get back stack depth
     */
    int getStackDepth() const { return _stackDepth; }

    /**
     * Check if we can go back
     */
    bool canGoBack() const { return _stackDepth > 0; }

    /**
     * Update status bar battery level
     */
    void setBatteryPercent(uint8_t percent);

    /**
     * Update status bar LoRa status
     */
    void setLoRaStatus(bool connected, int16_t rssi = 0);

    /**
     * Update status bar GPS status
     */
    void setGpsStatus(bool hasGps, bool hasFix = false);

    /**
     * Update status bar time
     */
    void setTime(uint32_t epochTime);

    /**
     * Set timezone offset from UTC
     * @param hours Offset in hours (e.g., -6 for CST, -8 for PST)
     */
    void setTimezoneOffset(int8_t hours);

    /**
     * Set node name for status bar
     */
    void setNodeName(const char* name);

    /**
     * Show temporary status message
     */
    void showStatus(const char* message, uint32_t durationMs = 2000);

    /**
     * Set notification count
     */
    void setNotificationCount(uint8_t count);

private:
    ScreenManager() = default;
    ~ScreenManager() = default;
    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    // Find screen by ID
    Screen* findScreen(ScreenId id) const;

    // Draw the current screen with chrome (status bar, soft keys)
    void drawScreen(bool fullRedraw);

    // Registered screens
    static constexpr int MAX_SCREENS = 16;
    Screen* _screens[MAX_SCREENS] = { nullptr };
    int _screenCount = 0;

    // Navigation stack
    ScreenId _backStack[MAX_SCREEN_STACK];
    int _stackDepth = 0;

    // Current state
    Screen* _currentScreen = nullptr;
    uint32_t _lastUpdateTime = 0;
    bool _forceRedraw = true;
    bool _initialized = false;
};

// Convenience macro for getting the manager
#define Screens ScreenManager::instance()

#endif // MESHBERRY_SCREENMANAGER_H
