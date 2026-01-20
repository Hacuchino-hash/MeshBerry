/**
 * MeshBerry Chat Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "ChatScreen.h"
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
ChatScreen* ChatScreen::_instance = nullptr;

void ChatScreen::setChannel(int channelIdx) {
    _channelIdx = channelIdx;

    // Get channel name from settings
    ChannelSettings& channels = SettingsManager::getChannelSettings();
    if (channelIdx >= 0 && channelIdx < channels.numChannels) {
        strncpy(_channelName, channels.channels[channelIdx].name, sizeof(_channelName) - 1);
        _channelName[sizeof(_channelName) - 1] = '\0';
    } else {
        strcpy(_channelName, "Unknown");
    }
}

void ChatScreen::onEnter() {
    _instance = this;
    _messageCount = 0;
    _scrollOffset = 0;
    _inputPos = 0;
    _inputBuffer[0] = '\0';
    _inputMode = false;
    requestRedraw();
}

void ChatScreen::onExit() {
    _instance = nullptr;
}

const char* ChatScreen::getTitle() const {
    return _channelName;
}

void ChatScreen::configureSoftKeys() {
    if (_inputMode) {
        SoftKeyBar::setLabels("Clear", "Send", "Cancel");
    } else {
        SoftKeyBar::setLabels("Type", nullptr, "Back");
    }
}

void ChatScreen::draw(bool fullRedraw) {
    if (fullRedraw) {
        // Clear content area
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Draw header with channel name
        Display::fillRect(0, Theme::CONTENT_Y, Theme::SCREEN_WIDTH, 24, Theme::BG_SECONDARY);
        Display::drawBitmap(8, Theme::CONTENT_Y + 4, Icons::CHANNEL_ICON, 16, 16, Theme::ACCENT);
        Display::drawText(28, Theme::CONTENT_Y + 8, _channelName, Theme::WHITE, 1);

        // Divider
        Display::drawHLine(0, Theme::CONTENT_Y + 24, Theme::SCREEN_WIDTH, Theme::DIVIDER);
    }

    drawMessages(fullRedraw);
    drawInputBar();
}

void ChatScreen::drawMessages(bool fullRedraw) {
    // Message area: from header to input bar
    int16_t msgY = Theme::CONTENT_Y + 26;
    int16_t msgHeight = Theme::CONTENT_HEIGHT - 26 - 28;  // Header and input bar
    int16_t lineHeight = 12;  // Each message line

    // Clear message area
    Display::fillRect(0, msgY, Theme::SCREEN_WIDTH, msgHeight, Theme::BG_PRIMARY);

    if (_messageCount == 0) {
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
    int startIdx = _messageCount - visibleLines - _scrollOffset;
    if (startIdx < 0) startIdx = 0;

    int16_t y = msgY + 2;
    for (int i = startIdx; i < _messageCount && y < msgY + msgHeight - lineHeight; i++) {
        const ChatMessage& msg = _messages[i];

        // Format: "Sender: message" or "> message" for outgoing
        char line[160];
        if (msg.isOutgoing) {
            snprintf(line, sizeof(line), "> %s", msg.text);
        } else {
            snprintf(line, sizeof(line), "%s: %s", msg.sender, msg.text);
        }

        // Truncate to fit screen
        const char* displayText = Theme::truncateText(line, Theme::SCREEN_WIDTH - 16, 1);

        // Color: outgoing = accent, incoming = white
        uint16_t textColor = msg.isOutgoing ? Theme::ACCENT : Theme::WHITE;
        Display::drawText(8, y, displayText, textColor, 1);

        y += lineHeight;
    }

    // Scroll indicator if needed
    if (_messageCount > visibleLines) {
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

void ChatScreen::drawInputBar() {
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

bool ChatScreen::handleInput(const InputData& input) {
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
                // Go back to messages
                Screens.goBack();
                return true;

            case InputEvent::TRACKBALL_UP:
                // Scroll up
                if (_scrollOffset < _messageCount - getVisibleMessageCount()) {
                    _scrollOffset++;
                    requestRedraw();
                }
                return true;

            case InputEvent::TRACKBALL_DOWN:
                // Scroll down
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

int ChatScreen::getVisibleMessageCount() const {
    int16_t msgHeight = Theme::CONTENT_HEIGHT - 26 - 28;
    return msgHeight / 12;
}

void ChatScreen::sendMessage() {
    if (_inputPos == 0 || !theMesh) return;

    // Send to channel
    if (theMesh->sendToChannel(_channelIdx, _inputBuffer)) {
        // Add to local display
        addMessage(theMesh->getNodeName(), _inputBuffer, millis() / 1000, true);
    }

    // Clear input and exit input mode
    clearInput();
    _inputMode = false;
    configureSoftKeys();
    SoftKeyBar::redraw();
    requestRedraw();
}

void ChatScreen::clearInput() {
    _inputBuffer[0] = '\0';
    _inputPos = 0;
}

void ChatScreen::addMessage(const char* sender, const char* text, uint32_t timestamp, bool isOutgoing) {
    // Shift messages if full
    if (_messageCount >= MAX_CHAT_MESSAGES) {
        for (int i = 0; i < MAX_CHAT_MESSAGES - 1; i++) {
            _messages[i] = _messages[i + 1];
        }
        _messageCount = MAX_CHAT_MESSAGES - 1;
    }

    // Add new message
    ChatMessage& msg = _messages[_messageCount++];
    strncpy(msg.sender, sender, sizeof(msg.sender) - 1);
    msg.sender[sizeof(msg.sender) - 1] = '\0';
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';
    msg.timestamp = timestamp;
    msg.isOutgoing = isOutgoing;

    // Auto-scroll to bottom
    _scrollOffset = 0;

    requestRedraw();
}

void ChatScreen::parseSenderAndText(const char* combined, char* sender, size_t senderLen,
                                     char* text, size_t textLen) {
    // Format: "SenderName: message"
    const char* colon = strchr(combined, ':');
    if (colon && colon > combined) {
        size_t nameLen = colon - combined;
        if (nameLen >= senderLen) nameLen = senderLen - 1;
        strncpy(sender, combined, nameLen);
        sender[nameLen] = '\0';

        // Skip ": " after colon
        const char* msgStart = colon + 1;
        while (*msgStart == ' ') msgStart++;
        strncpy(text, msgStart, textLen - 1);
        text[textLen - 1] = '\0';
    } else {
        // No colon found, treat whole thing as text
        strcpy(sender, "Unknown");
        strncpy(text, combined, textLen - 1);
        text[textLen - 1] = '\0';
    }
}

void ChatScreen::addToCurrentChat(int channelIdx, const char* senderAndText, uint32_t timestamp) {
    if (_instance && _instance->_channelIdx == channelIdx) {
        // Parse sender and text
        char sender[16];
        char text[128];
        parseSenderAndText(senderAndText, sender, sizeof(sender), text, sizeof(text));

        _instance->addMessage(sender, text, timestamp, false);
    }
}
