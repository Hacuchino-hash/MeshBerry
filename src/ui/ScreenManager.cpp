/**
 * MeshBerry Screen Manager Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "ScreenManager.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"

ScreenManager& ScreenManager::instance() {
    static ScreenManager instance;
    return instance;
}

void ScreenManager::init() {
    if (_initialized) return;

    // Initialize UI components
    StatusBar::init();
    SoftKeyBar::init();

    // Clear navigation stack
    _stackDepth = 0;
    for (int i = 0; i < MAX_SCREEN_STACK; i++) {
        _backStack[i] = ScreenId::NONE;
    }

    _currentScreen = nullptr;
    _forceRedraw = true;
    _lastUpdateTime = millis();
    _initialized = true;

    Serial.println("[UI] ScreenManager initialized");
}

void ScreenManager::registerScreen(Screen* screen) {
    if (!screen) return;

    if (_screenCount >= MAX_SCREENS) {
        Serial.println("[UI] Warning: Max screens reached");
        return;
    }

    // Check if already registered
    for (int i = 0; i < _screenCount; i++) {
        if (_screens[i] == screen || _screens[i]->getId() == screen->getId()) {
            Serial.printf("[UI] Screen %d already registered\n", (int)screen->getId());
            return;
        }
    }

    _screens[_screenCount++] = screen;
    Serial.printf("[UI] Registered screen %d (total: %d)\n", (int)screen->getId(), _screenCount);
}

Screen* ScreenManager::findScreen(ScreenId id) const {
    for (int i = 0; i < _screenCount; i++) {
        if (_screens[i] && _screens[i]->getId() == id) {
            return _screens[i];
        }
    }
    return nullptr;
}

void ScreenManager::navigateTo(ScreenId id, bool pushToStack) {
    Screen* newScreen = findScreen(id);
    if (!newScreen) {
        Serial.printf("[UI] Screen %d not found\n", (int)id);
        return;
    }

    // Push current screen to stack if requested
    if (pushToStack && _currentScreen && _stackDepth < MAX_SCREEN_STACK) {
        _backStack[_stackDepth++] = _currentScreen->getId();
    }

    // Exit current screen
    if (_currentScreen) {
        _currentScreen->onExit();
    }

    // Switch to new screen
    _currentScreen = newScreen;
    _currentScreen->onEnter();
    _currentScreen->configureSoftKeys();
    _forceRedraw = true;

    Serial.printf("[UI] Navigated to screen %d (stack depth: %d)\n", (int)id, _stackDepth);
}

bool ScreenManager::goBack() {
    if (_stackDepth <= 0) {
        return false;
    }

    ScreenId prevId = _backStack[--_stackDepth];
    Screen* prevScreen = findScreen(prevId);

    if (!prevScreen) {
        Serial.printf("[UI] Back screen %d not found\n", (int)prevId);
        return false;
    }

    // Exit current screen
    if (_currentScreen) {
        _currentScreen->onExit();
    }

    // Switch to previous screen
    _currentScreen = prevScreen;
    _currentScreen->onEnter();
    _currentScreen->configureSoftKeys();
    _forceRedraw = true;

    Serial.printf("[UI] Went back to screen %d (stack depth: %d)\n", (int)prevId, _stackDepth);
    return true;
}

void ScreenManager::goHome() {
    // Clear navigation stack
    _stackDepth = 0;

    // Navigate to home without pushing to stack
    navigateTo(ScreenId::HOME, false);
}

ScreenId ScreenManager::getCurrentScreenId() const {
    return _currentScreen ? _currentScreen->getId() : ScreenId::NONE;
}

void ScreenManager::handleInput(const InputData& input) {
    if (input.event == InputEvent::NONE) return;

    // Handle BACK and SOFTKEY_RIGHT globally (both act as back)
    if (input.event == InputEvent::BACK || input.event == InputEvent::SOFTKEY_RIGHT) {
        // First let the screen try to handle it (for in-screen back navigation)
        if (_currentScreen && _currentScreen->handleInput(input)) {
            if (_currentScreen->needsRedraw()) {
                drawScreen(false);
            }
            return;
        }
        // Screen didn't consume it, try global back
        if (canGoBack()) {
            goBack();
            return;
        }
        // At root, nothing to do
        return;
    }

    // Route other events to current screen
    if (_currentScreen) {
        if (_currentScreen->handleInput(input)) {
            // Screen consumed the event
            if (_currentScreen->needsRedraw()) {
                drawScreen(false);
            }
        }
    }
}

void ScreenManager::handleKey(uint8_t keyCode, char keyChar) {
    InputData input;

    // Map keys to soft key events
    if (keyCode == KEY_ESC || keyCode == KEY_BACKSPACE) {
        // Right soft key: Back
        input.event = InputEvent::SOFTKEY_RIGHT;
        SoftKeyBar::setHighlight(SoftKeyBar::KEY_RIGHT, true);
    } else if (keyCode == KEY_ENTER) {
        // Center soft key: Select/Open (Enter key only)
        // NOTE: Space is NOT mapped here - it should pass through as regular key
        // so screens with text input (ChatScreen, etc.) can use it for typing
        input.event = InputEvent::SOFTKEY_CENTER;
        SoftKeyBar::setHighlight(SoftKeyBar::KEY_CENTER, true);
    } else if (keyChar == 'q' || keyChar == 'Q' || keyChar == '$') {
        // Left soft key: Action (Q key or Sym+4)
        input.event = InputEvent::SOFTKEY_LEFT;
        SoftKeyBar::setHighlight(SoftKeyBar::KEY_LEFT, true);
    } else {
        // Regular key press
        input.event = InputEvent::KEY_PRESS;
        input.keyCode = keyCode;
        input.keyChar = keyChar;
    }

    handleInput(input);
}

void ScreenManager::handleTrackball(bool up, bool down, bool left, bool right, bool click) {
    InputData input;

    // Only one event at a time
    if (up) {
        input.event = InputEvent::TRACKBALL_UP;
    } else if (down) {
        input.event = InputEvent::TRACKBALL_DOWN;
    } else if (left) {
        input.event = InputEvent::TRACKBALL_LEFT;
    } else if (right) {
        input.event = InputEvent::TRACKBALL_RIGHT;
    } else if (click) {
        input.event = InputEvent::TRACKBALL_CLICK;
    } else {
        return;  // No event
    }

    handleInput(input);
}

void ScreenManager::update() {
    uint32_t now = millis();
    uint32_t deltaMs = now - _lastUpdateTime;
    _lastUpdateTime = now;

    // Update current screen
    if (_currentScreen) {
        _currentScreen->update(deltaMs);
    }

    // Check if redraw needed
    bool needsRedraw = _forceRedraw ||
                       StatusBar::needsUpdate() ||
                       (_currentScreen && _currentScreen->needsRedraw());

    if (needsRedraw) {
        drawScreen(_forceRedraw);
        _forceRedraw = false;
    }
}

void ScreenManager::forceRedraw() {
    _forceRedraw = true;
}

void ScreenManager::drawScreen(bool fullRedraw) {
    // Draw status bar
    if (fullRedraw) {
        StatusBar::redraw();
    } else {
        StatusBar::draw();
    }

    // Draw screen content
    if (_currentScreen) {
        if (fullRedraw) {
            // Clear content area
            Display::fillRect(0, Theme::CONTENT_Y,
                              Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                              Theme::BG_PRIMARY);
        }
        _currentScreen->draw(fullRedraw);
        _currentScreen->clearRedrawFlag();
    }

    // Draw soft key bar
    if (fullRedraw) {
        SoftKeyBar::redraw();
    } else {
        SoftKeyBar::draw();
    }
}

// =============================================================================
// STATUS BAR CONVENIENCE METHODS
// =============================================================================

void ScreenManager::setBatteryPercent(uint8_t percent) {
    StatusBar::setBatteryPercent(percent);
}

void ScreenManager::setLoRaStatus(bool connected, int16_t rssi) {
    StatusBar::setLoRaStatus(connected, rssi);
}

void ScreenManager::setGpsStatus(bool hasGps, bool hasFix) {
    StatusBar::setGpsStatus(hasGps, hasFix);
}

void ScreenManager::setTime(uint32_t epochTime) {
    StatusBar::setTime(epochTime);
}

void ScreenManager::setTimezoneOffset(int8_t hours) {
    StatusBar::setTimezoneOffset(hours);
}

void ScreenManager::setNodeName(const char* name) {
    StatusBar::setNodeName(name);
}

void ScreenManager::showStatus(const char* message, uint32_t durationMs) {
    StatusBar::showStatus(message, durationMs);
}

void ScreenManager::setNotificationCount(uint8_t count) {
    StatusBar::setNotificationCount(count);
}
