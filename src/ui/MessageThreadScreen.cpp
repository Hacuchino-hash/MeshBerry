/**
 * MeshBerry Message Thread Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "MessageThreadScreen.h"
#include "SoftKeyBar.h"
#include "../drivers/display.h"
#include "../settings/SettingsManager.h"
#include "../settings/DMSettings.h"

MessageThreadScreen* MessageThreadScreen::_instance = nullptr;

MessageThreadScreen::MessageThreadScreen() {
    _type = TYPE_CHANNEL;
    _channelIdx = -1;
    _contactId = 0;
    _title[0] = '\0';
    _totalMessages = 0;
    _cacheStartIdx = 0;
    _cacheValid = false;
    _renderCacheValid = false;
    _contentHeight = 0;
    _scrollY = 0;
    _autoScroll = true;
    _inputBuffer[0] = '\0';
    _inputPos = 0;
    _cursorVisible = false;
    _lastCursorBlink = 0;
    _instance = this;
}

void MessageThreadScreen::setConversation(ConversationType type, int channelIdx, uint32_t contactId, const char* title) {
    _type = type;
    _channelIdx = channelIdx;
    _contactId = contactId;
    strncpy(_title, title, 31);
    _title[31] = '\0';
}

void MessageThreadScreen::onEnter() {
    // Count total messages
    if (_type == TYPE_CHANNEL) {
        _totalMessages = MessageArchive::getChannelMessageCount(_channelIdx);
    } else {
        DMSettings& dms = SettingsManager::getDMSettings();
        DMConversation* conv = dms.getConversation(_contactId);
        _totalMessages = conv ? conv->messageCount : 0;
    }

    // Load most recent messages
    int loadCount = min(_totalMessages, CACHE_SIZE);
    int loadStart = max(0, _totalMessages - loadCount);
    loadMessages(loadStart, loadCount);

    // Auto-scroll to bottom
    _autoScroll = true;
    _scrollY = max(0, _contentHeight - 158);  // Scroll to bottom

    requestRedraw();
}

void MessageThreadScreen::onExit() {
    // Clear cache to free memory
    _messageCache.clear();
    _cacheValid = false;
    _renderCacheValid = false;
}

void MessageThreadScreen::configureSoftKeys() {
    SoftKeyBar::setLabels("Emoji", "Send", "Back");
}

void MessageThreadScreen::loadMessages(int startIdx, int count) {
    _messageCache.clear();
    _cacheStartIdx = startIdx;

    for (int i = 0; i < count && (startIdx + i) < _totalMessages; i++) {
        int msgIdx = startIdx + i;
        CachedMessage msg;
        msg.clear();

        if (_type == TYPE_CHANNEL) {
            // Load from MessageArchive
            ArchivedMessage archived;
            if (MessageArchive::getChannelMessage(_channelIdx, msgIdx, &archived)) {
                strncpy(msg.sender, archived.sender, 15);
                msg.sender[15] = '\0';
                strncpy(msg.text, archived.text, 127);
                msg.text[127] = '\0';
                msg.timestamp = archived.timestamp;
                msg.isOutgoing = archived.isOutgoing;
                msg.hops = archived.hops;
                _messageCache.push_back(msg);
            }
        } else {
            // Load from DMSettings
            DMSettings& dms = SettingsManager::getDMSettings();
            DMConversation* conv = dms.getConversation(_contactId);
            if (conv) {
                DMMessage* dmMsg = conv->getMessage(msgIdx);
                if (dmMsg) {
                    strncpy(msg.sender, dmMsg->isOutgoing ? "You" : _title, 15);
                    msg.sender[15] = '\0';
                    strncpy(msg.text, dmMsg->text, 127);
                    msg.text[127] = '\0';
                    msg.timestamp = dmMsg->timestamp;
                    msg.isOutgoing = dmMsg->isOutgoing;
                    msg.deliveryStatus = dmMsg->deliveryStatus;
                    msg.hops = 0;
                    _messageCache.push_back(msg);
                }
            }
        }
    }

    _cacheValid = true;
    _renderCacheValid = false;  // Trigger rebuild
}

void MessageThreadScreen::rebuildRenderCache() {
    _contentHeight = 0;
    const int CONTENT_WIDTH = 300;
    const int LINE_HEIGHT = 16;
    const int MESSAGE_GAP = 8;

    for (auto& msg : _messageCache) {
        // Wrap message text
        wrapMessageText(msg);

        // Calculate total height for this message
        int headerHeight = LINE_HEIGHT + 2;  // Sender + underline
        int textHeight = msg.wrappedLines.size() * LINE_HEIGHT;
        msg.totalHeight = headerHeight + textHeight + MESSAGE_GAP;
        _contentHeight += msg.totalHeight;
    }

    _renderCacheValid = true;
}

void MessageThreadScreen::wrapMessageText(CachedMessage& msg) {
    msg.wrappedLines.clear();

    // Simple word wrapping (max ~50 chars per line at size 2)
    const int MAX_LINE_CHARS = 24;  // Conservative for 300px width, size 2 font (12px char width)
    const char* text = msg.text;
    int len = strlen(text);
    int start = 0;

    while (start < len) {
        int end = start + MAX_LINE_CHARS;
        if (end >= len) {
            // Last chunk
            msg.wrappedLines.push_back(String(text + start));
            break;
        }

        // Find last space before end
        int lastSpace = -1;
        for (int i = start; i < end && i < len; i++) {
            if (text[i] == ' ') lastSpace = i;
        }

        if (lastSpace > start) {
            // Break at space
            String line(text + start, lastSpace - start);
            msg.wrappedLines.push_back(line);
            start = lastSpace + 1;  // Skip space
        } else {
            // No space found, force break
            String line(text + start, end - start);
            msg.wrappedLines.push_back(line);
            start = end;
        }
    }
}

void MessageThreadScreen::draw(bool fullRedraw) {
    const int16_t CONTENT_Y = Theme::CONTENT_Y + 28;
    const int16_t VISIBLE_HEIGHT = 158;  // 190 - 32 (input bar)
    const int16_t INPUT_Y = 178;

    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Title bar (modern card)
        const int16_t titleCardH = 24;
        Display::drawCard(4, Theme::CONTENT_Y + 2, Theme::SCREEN_WIDTH - 8, titleCardH,
                         Theme::COLOR_BG_ELEVATED, Theme::CARD_RADIUS, false);
        Display::fillGradient(4, Theme::CONTENT_Y + 2, Theme::SCREEN_WIDTH - 8, titleCardH,
                             Theme::COLOR_PRIMARY_DARK, Theme::COLOR_BG_ELEVATED);
        Display::drawText(12, Theme::CONTENT_Y + 6, _title, Theme::WHITE, 2);
    }

    // Rebuild render cache if needed
    if (!_renderCacheValid) {
        rebuildRenderCache();
    }

    // Draw messages
    drawMessages();

    // Always draw input bar
    drawInputBar();
}

void MessageThreadScreen::drawMessages() {
    const int16_t CONTENT_Y = Theme::CONTENT_Y + 28;
    const int16_t VISIBLE_HEIGHT = 158;

    // Clear message area
    Display::fillRect(0, CONTENT_Y, Theme::SCREEN_WIDTH, VISIBLE_HEIGHT, Theme::BG_PRIMARY);

    int16_t y = CONTENT_Y - _scrollY;

    for (auto& msg : _messageCache) {
        // Skip if completely above visible area
        if (y + msg.totalHeight < CONTENT_Y) {
            y += msg.totalHeight;
            continue;
        }

        // Stop if completely below visible area
        if (y > CONTENT_Y + VISIBLE_HEIGHT) {
            break;
        }

        // Draw sender header
        if (y >= CONTENT_Y && y < CONTENT_Y + VISIBLE_HEIGHT) {
            uint16_t senderColor = msg.isOutgoing ? Theme::COLOR_PRIMARY : Theme::COLOR_TEXT_SECONDARY;
            Display::drawText(10, y, msg.sender, senderColor, 1);

            // Time ago
            char timeStr[16];
            uint32_t age = (millis() / 1000) - msg.timestamp;
            if (age < 60) snprintf(timeStr, 16, "%ds", (int)age);
            else if (age < 3600) snprintf(timeStr, 16, "%dm", (int)(age / 60));
            else snprintf(timeStr, 16, "%dh", (int)(age / 3600));
            Display::drawText(240, y, timeStr, Theme::COLOR_TEXT_HINT, 1);

            // Delivery status for outgoing DMs
            if (msg.isOutgoing && _type == TYPE_DM) {
                const char* statusIcon = "";
                uint16_t statusColor = Theme::COLOR_TEXT_HINT;
                switch (msg.deliveryStatus) {
                    case 1: statusIcon = "..."; statusColor = Theme::COLOR_WARNING; break;  // Sending
                    case 2: statusIcon = "✓"; statusColor = Theme::COLOR_TEXT_HINT; break;  // Sent
                    case 3: statusIcon = "✓✓"; statusColor = Theme::COLOR_SUCCESS; break;  // Delivered
                    case 4: statusIcon = "✗"; statusColor = Theme::COLOR_ERROR; break;     // Failed
                }
                Display::drawText(270, y, statusIcon, statusColor, 1);
            }
        }

        y += 10;  // Move past header

        // Draw divider line
        if (y >= CONTENT_Y && y < CONTENT_Y + VISIBLE_HEIGHT) {
            Display::drawHLine(10, y, 300, Theme::COLOR_DIVIDER);
        }
        y += 2;

        // Draw wrapped text lines
        for (const auto& line : msg.wrappedLines) {
            if (y >= CONTENT_Y && y < CONTENT_Y + VISIBLE_HEIGHT) {
                Display::drawText(15, y, line.c_str(), Theme::COLOR_TEXT_PRIMARY, 2);
            }
            y += 16;
        }

        y += 8;  // Message gap
    }
}

void MessageThreadScreen::drawInputBar() {
    const int16_t INPUT_Y = 178;
    const int16_t INPUT_H = 32;

    // Background card
    Display::drawCard(4, INPUT_Y, Theme::SCREEN_WIDTH - 8, INPUT_H,
                     Theme::COLOR_BG_INPUT, Theme::INPUT_RADIUS_NEW, false);

    // Input text
    Display::drawText(10, INPUT_Y + 10, _inputBuffer, Theme::COLOR_TEXT_PRIMARY, 2);

    // Blinking cursor
    uint32_t now = millis();
    if (now - _lastCursorBlink > 500) {
        _cursorVisible = !_cursorVisible;
        _lastCursorBlink = now;
    }

    if (_cursorVisible) {
        int cursorX = 10 + (_inputPos * 12);  // 12px per char at size 2
        Display::fillRect(cursorX, INPUT_Y + 10, 2, 16, Theme::COLOR_FOCUS);
    }

    // Send button (if text present)
    if (_inputPos > 0) {
        Display::drawButton(Theme::SCREEN_WIDTH - 70, INPUT_Y + 4, 62, 24,
                           "Send", Display::BTN_NORMAL, true);
    }
}

bool MessageThreadScreen::handleInput(const InputData& input) {
    switch (input.event) {
        case InputEvent::TRACKBALL_UP:
            // Scroll up
            _scrollY -= 20;
            if (_scrollY < 0) _scrollY = 0;
            requestRedraw();
            return true;

        case InputEvent::TRACKBALL_DOWN:
            // Scroll down
            _scrollY += 20;
            int maxScroll = max(0, _contentHeight - 158);
            if (_scrollY > maxScroll) _scrollY = maxScroll;
            requestRedraw();
            return true;

        case InputEvent::KEY_PRESS:
            handleCharInput(input.keyChar);
            requestRedraw();
            return true;

        case InputEvent::BACKSPACE:
            handleBackspace();
            requestRedraw();
            return true;

        case InputEvent::ENTER:
        case InputEvent::SOFTKEY_CENTER:
            sendMessage();
            return true;

        case InputEvent::BACK:
        case InputEvent::SOFTKEY_RIGHT:
            Screens.goBack();
            return true;

        default:
            return false;
    }
}

void MessageThreadScreen::handleCharInput(char c) {
    if (c >= 32 && c < 127 && _inputPos < 127) {
        _inputBuffer[_inputPos++] = c;
        _inputBuffer[_inputPos] = '\0';
    }
}

void MessageThreadScreen::handleBackspace() {
    if (_inputPos > 0) {
        _inputPos--;
        _inputBuffer[_inputPos] = '\0';
    }
}

void MessageThreadScreen::sendMessage() {
    if (_inputPos == 0) return;

    // TODO: Actually send message via mesh
    Serial.printf("[MSG] Send to %s: %s\n", _title, _inputBuffer);

    // Clear input
    _inputBuffer[0] = '\0';
    _inputPos = 0;

    // Reload messages (will include newly sent message)
    onEnter();

    requestRedraw();
}

void MessageThreadScreen::update(uint32_t deltaMs) {
    // Cursor blink handled in draw()
}

void MessageThreadScreen::appendMessage(const char* sender, const char* text,
                                        uint32_t timestamp, bool isOutgoing, uint8_t hops) {
    // Add to cache if viewing this conversation
    CachedMessage msg;
    msg.clear();
    strncpy(msg.sender, sender, 15);
    msg.sender[15] = '\0';
    strncpy(msg.text, text, 127);
    msg.text[127] = '\0';
    msg.timestamp = timestamp;
    msg.isOutgoing = isOutgoing;
    msg.hops = hops;

    _messageCache.push_back(msg);
    _totalMessages++;
    _renderCacheValid = false;

    // Auto-scroll to bottom
    if (_autoScroll) {
        _scrollY = max(0, _contentHeight - 158);
    }

    requestRedraw();
}

bool MessageThreadScreen::isViewingConversation(ConversationType type, int channelIdx, uint32_t contactId) {
    if (_type != type) return false;
    if (type == TYPE_CHANNEL) return (_channelIdx == channelIdx);
    return (_contactId == contactId);
}

MessageThreadScreen* MessageThreadScreen::getInstance() {
    return _instance;
}
