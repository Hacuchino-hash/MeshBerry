/**
 * MeshBerry Chat Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "ChatScreen.h"
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
    } else {
        // Fresh entry - ALWAYS reset state (channel may have changed)
        _messageCount = 0;
        _scrollOffset = 0;
        _inputBuffer[0] = '\0';
        _inputPos = 0;
        _inputMode = false;

        // Load messages from archive for THIS channel
        ArchivedMessage* archived = new ArchivedMessage[MAX_CHAT_MESSAGES];
        if (archived) {
            int count = MessageArchive::loadChannelMessages(_channelIdx, archived, MAX_CHAT_MESSAGES);
            for (int i = 0; i < count; i++) {
                // Add directly to array without re-saving
                if (_messageCount < MAX_CHAT_MESSAGES) {
                    ChatMessage& msg = _messages[_messageCount++];
                    strncpy(msg.sender, archived[i].sender, sizeof(msg.sender) - 1);
                    msg.sender[sizeof(msg.sender) - 1] = '\0';
                    strncpy(msg.text, archived[i].text, sizeof(msg.text) - 1);
                    msg.text[sizeof(msg.text) - 1] = '\0';
                    msg.timestamp = archived[i].timestamp;
                    msg.isOutgoing = archived[i].isOutgoing;
                    msg.contentHash = 0;   // No hash for archived messages
                    msg.repeatCount = 0;
                    msg.hops = 0;          // No hop info in archive
                }
            }
            delete[] archived;
            Serial.printf("[CHAT] Loaded %d messages for channel %d\n", _messageCount, _channelIdx);
        }
    }

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
        SoftKeyBar::setLabels("Emoji", "Send", "Cancel");
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
    int16_t maxY = msgY + msgHeight;

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

    // Start from the most recent messages and work backward
    // We use _scrollOffset to skip the most recent N messages when scrolled
    int startIdx = (_messageCount - 1) - _scrollOffset;
    if (startIdx < 0) startIdx = 0;

    // First pass: figure out how many messages fit by working backward
    // For now, simpler approach: just render from startIdx going backward until we fill
    int16_t y = msgY + 2;
    for (int i = 0; i <= startIdx && y < maxY - 10; i++) {
        int msgIdx = startIdx - i + _scrollOffset;
        if (msgIdx < 0 || msgIdx >= _messageCount) continue;

        // Recalculate - we want oldest to newest, so start from (startIdx - visible) and go up
    }

    // Simpler approach: calculate visible count and render oldest to newest
    // Estimate ~3 lines per message average with wrapping
    int estVisibleMsgs = msgHeight / 30;  // Rough estimate
    int displayStartIdx = _messageCount - estVisibleMsgs - _scrollOffset;
    if (displayStartIdx < 0) displayStartIdx = 0;

    y = msgY + 2;
    for (int i = displayStartIdx; i < _messageCount && y < maxY - 10; i++) {
        const ChatMessage& msg = _messages[i];

        // Format: "Sender: message" or "> message" for outgoing
        char line[192];
        if (msg.isOutgoing) {
            snprintf(line, sizeof(line), "> %s", msg.text);
        } else {
            snprintf(line, sizeof(line), "%s: %s", msg.sender, msg.text);
        }

        // Wrap text into multiple lines
        char wrappedLines[4][64];
        int lineCount = Theme::wrapText(line, Theme::SCREEN_WIDTH - 20, 1, wrappedLines, 4);

        // Color: outgoing = accent, incoming = white
        uint16_t textColor = msg.isOutgoing ? Theme::ACCENT : Theme::WHITE;

        // Draw each wrapped line
        for (int ln = 0; ln < lineCount && y < maxY - 10; ln++) {
            Display::drawTextWithEmoji(8, y, wrappedLines[ln], textColor, 1);
            y += 10;
        }

        // Show metadata below message
        if (msg.isOutgoing && msg.repeatCount > 0) {
            // Show repeat count for outgoing messages
            char repeatStr[24];
            snprintf(repeatStr, sizeof(repeatStr), "  x%d repeats", msg.repeatCount);
            Display::drawText(8, y, repeatStr, Theme::GRAY_LIGHT, 1);
            y += 8;
        } else if (!msg.isOutgoing && msg.hops > 0) {
            // Show hop count for incoming messages
            char hopStr[24];
            snprintf(hopStr, sizeof(hopStr), "  %d hop%s", msg.hops, msg.hops > 1 ? "s" : "");
            Display::drawText(8, y, hopStr, Theme::GRAY_LIGHT, 1);
            y += 8;
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

bool ChatScreen::handleInput(const InputData& input) {
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
                // Normal mode: Type | (none) | Back
                if (tx >= 214) {
                    // Right soft key = Back
                    Screens.goBack();
                } else if (tx < 107) {
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
                // Back to messages (discard input)
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

    // Convert shortcodes like :smile: to emoji
    Emoji::convertShortcodes(_inputBuffer, sizeof(_inputBuffer));

    // Compute content hash BEFORE sending (for repeat tracking)
    uint32_t contentHash = hashMessage(_channelIdx, _inputBuffer);

    // Send to channel
    if (theMesh->sendToChannel(_channelIdx, _inputBuffer)) {
        // Add to local display with content hash for repeat tracking
        addMessage(theMesh->getNodeName(), _inputBuffer, millis() / 1000, true, 0);

        // Store the content hash in the most recently added message
        if (_messageCount > 0) {
            _messages[_messageCount - 1].contentHash = contentHash;
        }
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

void ChatScreen::addMessage(const char* sender, const char* text, uint32_t timestamp, bool isOutgoing, uint8_t hops) {
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
    msg.contentHash = 0;    // Will be set by sendMessage() for outgoing
    msg.repeatCount = 0;
    msg.hops = hops;

    // Persist to archive
    ArchivedMessage archived;
    archived.clear();
    archived.timestamp = timestamp;
    strncpy(archived.sender, sender, ARCHIVE_SENDER_LEN - 1);
    archived.sender[ARCHIVE_SENDER_LEN - 1] = '\0';
    strncpy(archived.text, text, ARCHIVE_TEXT_LEN - 1);
    archived.text[ARCHIVE_TEXT_LEN - 1] = '\0';
    archived.isOutgoing = isOutgoing ? 1 : 0;
    MessageArchive::saveChannelMessage(_channelIdx, archived);

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

void ChatScreen::addToCurrentChat(int channelIdx, const char* senderAndText, uint32_t timestamp, uint8_t hops) {
    if (_instance && _instance->_channelIdx == channelIdx) {
        // Parse sender and text
        char sender[16];
        char text[128];
        parseSenderAndText(senderAndText, sender, sizeof(sender), text, sizeof(text));

        _instance->addMessage(sender, text, timestamp, false, hops);
    }
}

void ChatScreen::updateRepeatCount(int channelIdx, uint32_t contentHash, uint8_t repeatCount) {
    if (!_instance || _instance->_channelIdx != channelIdx) return;

    // Search for message with matching hash (search from newest)
    for (int i = _instance->_messageCount - 1; i >= 0; i--) {
        ChatMessage& msg = _instance->_messages[i];
        if (msg.isOutgoing && msg.contentHash == contentHash) {
            msg.repeatCount = repeatCount;
            _instance->requestRedraw();
            Serial.printf("[CHAT] Updated repeat count for msg hash %08X to %d\n", contentHash, repeatCount);
            return;
        }
    }
}

uint32_t ChatScreen::hashMessage(int channelIdx, const char* text) {
    // FNV-1a hash - must match MeshBerryMesh::hashChannelMessage()
    uint32_t hash = 0x811c9dc5;  // FNV-1a offset basis
    const uint32_t prime = 0x01000193;  // FNV-1a prime

    // Mix in channel index
    hash ^= (uint32_t)channelIdx;
    hash *= prime;

    // Mix in text content
    if (text) {
        while (*text) {
            hash ^= (uint8_t)*text++;
            hash *= prime;
        }
    }

    return hash;
}
