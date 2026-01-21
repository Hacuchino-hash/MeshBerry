/**
 * MeshBerry DM Chat Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "DMChatScreen.h"
#include "DMSettingsScreen.h"
#include "SoftKeyBar.h"
#include "Icons.h"
#include "Emoji.h"
#include "EmojiPickerScreen.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../settings/SettingsManager.h"
#include "../settings/MessageArchive.h"
#include "../mesh/MeshBerryMesh.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// External mesh instance
extern MeshBerryMesh* theMesh;

// External emoji picker for getting selected emoji
extern EmojiPickerScreen emojiPickerScreen;

// External DM settings screen
extern DMSettingsScreen dmSettingsScreen;

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

    // Check if returning from emoji picker with a selection
    if (emojiPickerScreen.wasEmojiSelected()) {
        char emoji[32];  // Buffer for :shortcode: format
        if (emojiPickerScreen.getSelectedEmoji(emoji)) {
            // Append emoji shortcode to input buffer
            size_t emojiLen = strlen(emoji);
            if (_inputPos + emojiLen < sizeof(_inputBuffer) - 1) {
                memcpy(_inputBuffer + _inputPos, emoji, emojiLen);
                _inputPos += emojiLen;
                _inputBuffer[_inputPos] = '\0';
            }
        }
        // Stay in input mode after inserting emoji
        _inputMode = true;
    } else if (_inputPos == 0) {
        // Fresh entry - reset scroll state
        _scrollOffset = 0;
        _inputBuffer[0] = '\0';
        _inputMode = false;
    }

    // Mark conversation as read
    DMSettings& dms = SettingsManager::getDMSettings();
    int convIdx = dms.findConversation(_contactId);
    if (convIdx >= 0) {
        DMConversation* conv = dms.getConversation(convIdx);
        if (conv) {
            conv->markAsRead();

            // Update status bar notification badge
            int unread = dms.getTotalUnreadCount();
            Screens.setNotificationCount(unread);
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
        SoftKeyBar::setLabels("Emoji", "Send", "Cancel");
    } else {
        SoftKeyBar::setLabels("Type", "Route", "Back");
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
    int16_t maxY = msgY + msgHeight;

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

    // Estimate visible messages with wrapping (~3 lines per message average)
    int estVisibleMsgs = msgHeight / 30;
    int displayStartIdx = conv->messageCount - estVisibleMsgs - _scrollOffset;
    if (displayStartIdx < 0) displayStartIdx = 0;

    int16_t y = msgY + 2;
    for (int i = displayStartIdx; i < conv->messageCount && y < maxY - 10; i++) {
        const DMMessage* msg = conv->getMessage(i);
        if (!msg) continue;

        // Format: "> message [status]" for outgoing, "< message" for incoming
        char line[220];
        if (msg->isOutgoing) {
            // Add delivery status indicator for outgoing messages
            const char* statusStr = "";
            switch (msg->status) {
                case DM_STATUS_SENDING:   statusStr = " ..."; break;
                case DM_STATUS_SENT:      statusStr = " [Sent]"; break;
                case DM_STATUS_DELIVERED: statusStr = " [OK]"; break;
                case DM_STATUS_FAILED:    statusStr = " [FAIL]"; break;
            }
            snprintf(line, sizeof(line), "> %s%s", msg->text, statusStr);
        } else {
            snprintf(line, sizeof(line), "< %s", msg->text);
        }

        // Wrap text into multiple lines
        char wrappedLines[4][64];
        int lineCount = Theme::wrapText(line, Theme::SCREEN_WIDTH - 20, 1, wrappedLines, 4);

        // Color: outgoing = accent (blue), incoming = green, failed = red
        uint16_t textColor;
        if (msg->isOutgoing) {
            if (msg->status == DM_STATUS_FAILED) {
                textColor = Theme::RED;
            } else if (msg->status == DM_STATUS_DELIVERED) {
                textColor = Theme::GREEN;  // Green for confirmed delivery
            } else {
                textColor = Theme::ACCENT;  // Blue for sending/sent
            }
        } else {
            textColor = Theme::GREEN;
        }

        // Draw each wrapped line
        for (int ln = 0; ln < lineCount && y < maxY - 10; ln++) {
            Display::drawTextWithEmoji(8, y, wrappedLines[ln], textColor, 1);
            y += 10;
        }

        y += 2;  // Gap between messages
    }

    // Scroll indicators
    if (_scrollOffset > 0) {
        Display::drawBitmap(Theme::SCREEN_WIDTH - 12, msgY + 2,
                           Icons::ARROW_UP, 8, 8, Theme::GRAY_LIGHT);
    }
    if (displayStartIdx > 0) {
        Display::drawBitmap(Theme::SCREEN_WIDTH - 12, maxY - 10,
                           Icons::ARROW_DOWN, 8, 8, Theme::GRAY_LIGHT);
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
        // Use drawTextWithEmoji to properly render emoji glyphs
        Display::drawTextWithEmoji(fieldX + 4, fieldY + 6, displayText, Theme::WHITE, 1);

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
    // Handle touch tap for soft keys (works in both modes)
    if (input.event == InputEvent::TOUCH_TAP) {
        int16_t ty = input.touchY;
        int16_t tx = input.touchX;

        // Soft key bar touch (Y >= 210)
        if (ty >= Theme::SOFTKEY_BAR_Y) {
            if (_inputMode) {
                // Input mode: Emoji | Send | Cancel
                if (tx >= 214) {
                    // Right soft key = Cancel/Back
                    Screens.goBack();
                } else if (tx >= 107) {
                    // Center soft key = Send
                    if (_inputPos > 0) {
                        sendMessage();
                    }
                } else {
                    // Left soft key = Emoji picker
                    Screens.navigateTo(ScreenId::EMOJI_PICKER);
                }
            } else {
                // Normal mode: Type | Route | Back
                if (tx >= 214) {
                    // Right soft key = Back
                    Screens.goBack();
                } else if (tx >= 107) {
                    // Center soft key = Route settings
                    dmSettingsScreen.setContact(_contactId);
                    Screens.navigateTo(ScreenId::DM_SETTINGS);
                } else {
                    // Left soft key = Type (enter input mode)
                    _inputMode = true;
                    configureSoftKeys();
                    SoftKeyBar::redraw();
                    requestRedraw();
                }
            }
            return true;
        }
        return true;
    }

    if (_inputMode) {
        // In text input mode
        switch (input.event) {
            case InputEvent::SOFTKEY_LEFT:
                // Open emoji picker
                Screens.navigateTo(ScreenId::EMOJI_PICKER);
                return true;

            case InputEvent::SOFTKEY_CENTER:
                // Send message
                if (_inputPos > 0) {
                    sendMessage();
                }
                return true;

            case InputEvent::SOFTKEY_RIGHT:
            case InputEvent::BACK:
                // Back to contacts (discard input)
                Screens.goBack();
                return true;

            case InputEvent::TRACKBALL_CLICK:
                // Space key comes in as TRACKBALL_CLICK (workaround for T-Deck GPIO 0 issue)
                // Insert space character when in input mode
                if (_inputPos < (int)sizeof(_inputBuffer) - 1) {
                    _inputBuffer[_inputPos++] = ' ';
                    _inputBuffer[_inputPos] = '\0';
                    requestRedraw();
                }
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

            case InputEvent::SOFTKEY_CENTER:
                // Open DM routing settings
                dmSettingsScreen.setContact(_contactId);
                Screens.navigateTo(ScreenId::DM_SETTINGS);
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

    // Convert shortcodes like :smile: to emoji
    Emoji::convertShortcodes(_inputBuffer, sizeof(_inputBuffer));

    // Send via mesh - get ACK CRC for delivery tracking
    uint32_t ack_crc = 0;
    if (theMesh->sendDirectMessage(_contactId, _inputBuffer, &ack_crc)) {
        // Store in DMSettings as outgoing with ACK CRC for status tracking
        uint32_t timestamp = millis() / 1000;  // Simple timestamp
        if (theMesh->getRTCClock()) {
            timestamp = theMesh->getRTCClock()->getCurrentTime();
        }

        DMSettings& dms = SettingsManager::getDMSettings();
        dms.addMessage(_contactId, _inputBuffer, true, timestamp, ack_crc);

        // Also persist to archive for extended history
        ArchivedMessage archived;
        archived.clear();
        archived.timestamp = timestamp;
        strncpy(archived.sender, theMesh->getNodeName(), ARCHIVE_SENDER_LEN - 1);
        archived.sender[ARCHIVE_SENDER_LEN - 1] = '\0';
        strncpy(archived.text, _inputBuffer, ARCHIVE_TEXT_LEN - 1);
        archived.text[ARCHIVE_TEXT_LEN - 1] = '\0';
        archived.isOutgoing = 1;
        MessageArchive::saveDMMessage(_contactId, archived);

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
