/**
 * MeshBerry DM Chat Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "DMChatScreen.h"
#include "SoftKeyBar.h"
#include "Icons.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../settings/SettingsManager.h"
#include "../mesh/MeshBerryMesh.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// External mesh instance
extern MeshBerryMesh* theMesh;

// Static member
DMChatScreen* DMChatScreen::_instance = nullptr;

void DMChatScreen::setContact(uint32_t contactId, const char* contactName) {
    _contactId = contactId;
    if (contactName) {
        strncpy(_contactName, contactName, sizeof(_contactName) - 1);
        _contactName[sizeof(_contactName) - 1] = '\0';
    } else {
        snprintf(_contactName, sizeof(_contactName), "Node %08X", contactId);
    }
}

void DMChatScreen::onEnter() {
    _instance = this;
    _scrollOffset = 0;
    _inputPos = 0;
    _inputBuffer[0] = '\0';
    _inputMode = false;

    // Mark conversation as read
    DMSettings& dms = SettingsManager::getDMSettings();
    int convIdx = dms.findConversation(_contactId);
    if (convIdx >= 0) {
        DMConversation* conv = dms.getConversation(convIdx);
        if (conv) {
            conv->markAsRead();
        }
    }

    requestRedraw();
}

void DMChatScreen::onExit() {
    _instance = nullptr;

    // Save DM settings when leaving chat
    SettingsManager::saveDMs();
}

const char* DMChatScreen::getTitle() const {
    return _contactName;
}

void DMChatScreen::configureSoftKeys() {
    if (_inputMode) {
        SoftKeyBar::setLabels("Clear", "Send", "Cancel");
    } else {
        SoftKeyBar::setLabels("Type", nullptr, "Back");
    }
}

void DMChatScreen::draw(bool fullRedraw) {
    if (fullRedraw) {
        // Clear content area
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Draw header with contact name
        Display::fillRect(0, Theme::CONTENT_Y, Theme::SCREEN_WIDTH, 24, Theme::BG_SECONDARY);
        Display::drawBitmap(8, Theme::CONTENT_Y + 4, Icons::CONTACTS_ICON, 16, 16, Theme::GREEN);
        Display::drawText(28, Theme::CONTENT_Y + 8, _contactName, Theme::WHITE, 1);

        // "DM" indicator
        Display::drawText(Theme::SCREEN_WIDTH - 28, Theme::CONTENT_Y + 8, "DM", Theme::ACCENT, 1);

        // Divider
        Display::drawHLine(0, Theme::CONTENT_Y + 24, Theme::SCREEN_WIDTH, Theme::DIVIDER);
    }

    drawMessages(fullRedraw);
    drawInputBar();
}

void DMChatScreen::drawMessages(bool fullRedraw) {
    // Message area: from header to input bar
    int16_t msgY = Theme::CONTENT_Y + 26;
    int16_t msgHeight = Theme::CONTENT_HEIGHT - 26 - 28;  // Header and input bar
    int16_t lineHeight = 12;

    // Clear message area
    Display::fillRect(0, msgY, Theme::SCREEN_WIDTH, msgHeight, Theme::BG_PRIMARY);

    // Get conversation from DMSettings
    DMSettings& dms = SettingsManager::getDMSettings();
    int convIdx = dms.findConversation(_contactId);

    if (convIdx < 0) {
        Display::drawTextCentered(0, msgY + msgHeight / 2 - 8,
                                  Theme::SCREEN_WIDTH,
                                  "No messages yet", Theme::TEXT_SECONDARY, 1);
        Display::drawTextCentered(0, msgY + msgHeight / 2 + 4,
                                  Theme::SCREEN_WIDTH,
                                  "Type to send", Theme::GRAY_LIGHT, 1);
        return;
    }

    const DMConversation* conv = dms.getConversation(convIdx);
    if (!conv || conv->messageCount == 0) {
        Display::drawTextCentered(0, msgY + msgHeight / 2 - 8,
                                  Theme::SCREEN_WIDTH,
                                  "No messages yet", Theme::TEXT_SECONDARY, 1);
        Display::drawTextCentered(0, msgY + msgHeight / 2 + 4,
                                  Theme::SCREEN_WIDTH,
                                  "Type to send", Theme::GRAY_LIGHT, 1);
        return;
    }

    // Calculate how many messages we can show
    int visibleLines = msgHeight / lineHeight;
    int startIdx = conv->messageCount - visibleLines - _scrollOffset;
    if (startIdx < 0) startIdx = 0;

    int16_t y = msgY + 2;
    for (int i = startIdx; i < conv->messageCount && y < msgY + msgHeight - lineHeight; i++) {
        const DMMessage* msg = conv->getMessage(i);
        if (!msg) continue;

        // Format: "> message" for outgoing, "< message" for incoming
        char line[160];
        if (msg->isOutgoing) {
            snprintf(line, sizeof(line), "> %s", msg->text);
        } else {
            snprintf(line, sizeof(line), "< %s", msg->text);
        }

        // Truncate to fit screen
        const char* displayText = Theme::truncateText(line, Theme::SCREEN_WIDTH - 16, 1);

        // Color: outgoing = accent (blue), incoming = green
        uint16_t textColor = msg->isOutgoing ? Theme::ACCENT : Theme::GREEN;
        Display::drawText(8, y, displayText, textColor, 1);

        y += lineHeight;
    }

    // Scroll indicator if needed
    if (conv->messageCount > visibleLines) {
        if (_scrollOffset > 0) {
            Display::drawBitmap(Theme::SCREEN_WIDTH - 12, msgY + 2,
                               Icons::ARROW_UP, 8, 8, Theme::GRAY_LIGHT);
        }
        if (startIdx > 0) {
            Display::drawBitmap(Theme::SCREEN_WIDTH - 12, msgY + msgHeight - 10,
                               Icons::ARROW_DOWN, 8, 8, Theme::GRAY_LIGHT);
        }
    }
}

void DMChatScreen::drawInputBar() {
    int16_t inputY = Theme::SOFTKEY_BAR_Y - 28;

    // Input bar background
    Display::fillRect(0, inputY, Theme::SCREEN_WIDTH, 28, Theme::BG_SECONDARY);
    Display::drawHLine(0, inputY, Theme::SCREEN_WIDTH, Theme::DIVIDER);

    // Input field
    int16_t fieldX = 8;
    int16_t fieldY = inputY + 4;
    int16_t fieldW = Theme::SCREEN_WIDTH - 16;
    int16_t fieldH = 20;

    uint16_t bgColor = _inputMode ? Theme::GRAY_DARK : Theme::BG_SECONDARY;
    uint16_t borderColor = _inputMode ? Theme::ACCENT : Theme::GRAY_MID;

    Display::fillRoundRect(fieldX, fieldY, fieldW, fieldH, 4, bgColor);
    Display::drawRoundRect(fieldX, fieldY, fieldW, fieldH, 4, borderColor);

    // Input text or placeholder
    if (_inputPos > 0) {
        // Show input text (truncate from right if too long)
        int maxChars = (fieldW - 8) / 6;  // 6 pixels per char
        const char* displayText = _inputBuffer;
        if (_inputPos > maxChars) {
            displayText = _inputBuffer + (_inputPos - maxChars);
        }
        Display::drawText(fieldX + 4, fieldY + 6, displayText, Theme::WHITE, 1);

        // Cursor (blinking)
        if (_inputMode && (millis() / 500) % 2 == 0) {
            int16_t cursorX = fieldX + 4 + strlen(displayText) * 6;
            if (cursorX < fieldX + fieldW - 4) {
                Display::drawVLine(cursorX, fieldY + 4, fieldH - 8, Theme::WHITE);
            }
        }
    } else {
        Display::drawText(fieldX + 4, fieldY + 6, "Type message...", Theme::GRAY_LIGHT, 1);
    }
}

bool DMChatScreen::handleInput(const InputData& input) {
    if (_inputMode) {
        // In text input mode
        switch (input.event) {
            case InputEvent::SOFTKEY_LEFT:
                // Clear input
                clearInput();
                requestRedraw();
                return true;

            case InputEvent::SOFTKEY_CENTER:
                // Send message
                if (_inputPos > 0) {
                    sendMessage();
                }
                return true;

            case InputEvent::SOFTKEY_RIGHT:
                // Cancel input mode
                _inputMode = false;
                configureSoftKeys();
                SoftKeyBar::redraw();
                requestRedraw();
                return true;

            case InputEvent::KEY_PRESS:
                if (input.keyChar >= 32 && input.keyChar < 127) {
                    // Add character
                    if (_inputPos < (int)sizeof(_inputBuffer) - 1) {
                        _inputBuffer[_inputPos++] = input.keyChar;
                        _inputBuffer[_inputPos] = '\0';
                        requestRedraw();
                    }
                } else if (input.keyCode == KEY_BACKSPACE && _inputPos > 0) {
                    // Delete character
                    _inputBuffer[--_inputPos] = '\0';
                    requestRedraw();
                } else if (input.keyCode == KEY_ENTER && _inputPos > 0) {
                    // Send on Enter
                    sendMessage();
                }
                return true;

            case InputEvent::BACK:
                // Exit input mode
                _inputMode = false;
                configureSoftKeys();
                SoftKeyBar::redraw();
                requestRedraw();
                return true;

            default:
                return false;
        }
    } else {
        // Normal mode
        switch (input.event) {
            case InputEvent::SOFTKEY_LEFT:
            case InputEvent::TRACKBALL_CLICK:
                // Enter input mode
                _inputMode = true;
                configureSoftKeys();
                SoftKeyBar::redraw();
                requestRedraw();
                return true;

            case InputEvent::SOFTKEY_RIGHT:
            case InputEvent::BACK:
                // Go back
                Screens.goBack();
                return true;

            case InputEvent::TRACKBALL_UP:
                // Scroll up (show older messages)
                {
                    DMSettings& dms = SettingsManager::getDMSettings();
                    int convIdx = dms.findConversation(_contactId);
                    if (convIdx >= 0) {
                        const DMConversation* conv = dms.getConversation(convIdx);
                        if (conv && _scrollOffset < conv->messageCount - getVisibleMessageCount()) {
                            _scrollOffset++;
                            requestRedraw();
                        }
                    }
                }
                return true;

            case InputEvent::TRACKBALL_DOWN:
                // Scroll down (show newer messages)
                if (_scrollOffset > 0) {
                    _scrollOffset--;
                    requestRedraw();
                }
                return true;

            case InputEvent::KEY_PRESS:
                // Start typing (but not with space alone - require a real character)
                if (input.keyChar > 32 && input.keyChar < 127) {
                    _inputMode = true;
                    _inputBuffer[0] = input.keyChar;
                    _inputBuffer[1] = '\0';
                    _inputPos = 1;
                    configureSoftKeys();
                    SoftKeyBar::redraw();
                    requestRedraw();
                }
                return true;

            default:
                return false;
        }
    }

    return false;
}

int DMChatScreen::getVisibleMessageCount() const {
    int16_t msgHeight = Theme::CONTENT_HEIGHT - 26 - 28;
    return msgHeight / 12;
}

void DMChatScreen::sendMessage() {
    if (_inputPos == 0 || !theMesh) return;

    // Send via mesh
    if (theMesh->sendDirectMessage(_contactId, _inputBuffer)) {
        // Store in DMSettings as outgoing
        uint32_t timestamp = millis() / 1000;  // Simple timestamp
        if (theMesh->getRTCClock()) {
            timestamp = theMesh->getRTCClock()->getCurrentTime();
        }

        DMSettings& dms = SettingsManager::getDMSettings();
        dms.addMessage(_contactId, _inputBuffer, true, timestamp);

        // Auto-scroll to bottom
        _scrollOffset = 0;
    }

    // Clear input and exit input mode
    clearInput();
    _inputMode = false;
    configureSoftKeys();
    SoftKeyBar::redraw();
    requestRedraw();
}

void DMChatScreen::clearInput() {
    _inputBuffer[0] = '\0';
    _inputPos = 0;
}

void DMChatScreen::onMessageReceived(const char* text, uint32_t timestamp) {
    // Message already stored in DMSettings by the callback
    // Just need to refresh display and scroll to bottom
    _scrollOffset = 0;
    requestRedraw();
}

void DMChatScreen::addToCurrentChat(uint32_t contactId, const char* text, uint32_t timestamp) {
    if (_instance && _instance->_contactId == contactId) {
        _instance->onMessageReceived(text, timestamp);
    }
}
