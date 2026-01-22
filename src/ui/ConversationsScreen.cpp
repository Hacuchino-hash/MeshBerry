/**
 * MeshBerry Conversations Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "ConversationsScreen.h"
#include "SoftKeyBar.h"
#include "Icons.h"
#include "../drivers/display.h"
#include "../settings/SettingsManager.h"
#include "../settings/MessageArchive.h"
#include "../settings/DMSettings.h"
#include <algorithm>

// Singleton instance
ConversationsScreen* ConversationsScreen::_instance = nullptr;

ConversationsScreen::ConversationsScreen() {
    _count = 0;
    _selectedIdx = 0;
    _listView.setBounds(0, Theme::CONTENT_Y, Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT);
    _listView.setItemHeight(60);  // Taller items for preview text
    _instance = this;
}

void ConversationsScreen::onEnter() {
    buildConversationList();
    _selectedIdx = 0;
    requestRedraw();
}

void ConversationsScreen::onExit() {
    // Nothing to clean up
}

void ConversationsScreen::configureSoftKeys() {
    SoftKeyBar::setLabels("New", "Open", "Back");
}

void ConversationsScreen::buildConversationList() {
    _count = 0;

    // 1. Load all DM conversations from DMSettings
    DMSettings& dms = SettingsManager::getDMSettings();
    for (int i = 0; i < DMSettings::MAX_CONVERSATIONS && _count < MAX_CONVERSATIONS; i++) {
        DMConversation* conv = dms.getConversation(i);
        if (conv && conv->messageCount > 0 && conv->isActive) {
            _conversations[_count].type = TYPE_DM;
            _conversations[_count].contactId = conv->contactId;
            strncpy(_conversations[_count].contactName, conv->contactName, 31);
            _conversations[_count].contactName[31] = '\0';

            // Extract last message preview
            const DMMessage* lastMsg = conv->getMessage(conv->messageCount - 1);
            if (lastMsg) {
                strncpy(_conversations[_count].lastMessagePreview, lastMsg->text, 63);
                _conversations[_count].lastMessagePreview[63] = '\0';
                _conversations[_count].lastMessageTime = lastMsg->timestamp;
            }

            _conversations[_count].unreadCount = conv->unreadCount;
            _conversations[_count].channelIdx = -1;
            _count++;
        }
    }

    // 2. Load all channel conversations from MessageArchive
    for (int ch = 0; ch < 8 && _count < MAX_CONVERSATIONS; ch++) {
        int msgCount = MessageArchive::getChannelMessageCount(ch);
        if (msgCount > 0) {
            _conversations[_count].type = TYPE_CHANNEL;
            _conversations[_count].channelIdx = ch;
            _conversations[_count].contactId = 0;

            // Get channel name from settings
            ChannelSettings& channels = SettingsManager::getChannelSettings();
            strncpy(_conversations[_count].channelName, channels.channels[ch].name, 31);
            _conversations[_count].channelName[31] = '\0';

            // Extract last message from archive
            ArchivedMessage lastMsg;
            if (MessageArchive::getChannelMessage(ch, msgCount - 1, &lastMsg)) {
                char preview[64];
                snprintf(preview, 64, "%s: %s", lastMsg.sender, lastMsg.text);
                strncpy(_conversations[_count].lastMessagePreview, preview, 63);
                _conversations[_count].lastMessagePreview[63] = '\0';
                _conversations[_count].lastMessageTime = lastMsg.timestamp;
            }

            _conversations[_count].unreadCount = 0;  // TODO: Track channel unread count
            _count++;
        }
    }

    // 3. Sort by last message time (newest first)
    sortByLastMessage();

    // Update ListView item count
    _listView.setItemCount(_count);
}

void ConversationsScreen::sortByLastMessage() {
    // Simple bubble sort (small list, no need for qsort)
    for (int i = 0; i < _count - 1; i++) {
        for (int j = 0; j < _count - i - 1; j++) {
            if (_conversations[j].lastMessageTime < _conversations[j + 1].lastMessageTime) {
                // Swap
                UnifiedConversation temp = _conversations[j];
                _conversations[j] = _conversations[j + 1];
                _conversations[j + 1] = temp;
            }
        }
    }
}

void ConversationsScreen::draw(bool fullRedraw) {
    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Modern title card
        const int16_t titleCardH = 26;
        Display::drawCard(4, Theme::CONTENT_Y + 2, Theme::SCREEN_WIDTH - 8, titleCardH,
                         Theme::COLOR_BG_ELEVATED, Theme::CARD_RADIUS, false);
        Display::fillGradient(4, Theme::CONTENT_Y + 2, Theme::SCREEN_WIDTH - 8, titleCardH,
                             Theme::COLOR_PRIMARY_DARK, Theme::COLOR_BG_ELEVATED);
        Display::drawText(12, Theme::CONTENT_Y + 8, "Conversations", Theme::WHITE, 2);
    }

    // Adjust list bounds below title
    _listView.setBounds(0, Theme::CONTENT_Y + 32, Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT - 32);

    // Draw conversation list items
    int16_t startIdx = _listView.getScrollOffset();
    int16_t visibleCount = _listView.getVisibleItemCount();

    for (int i = 0; i < visibleCount && (startIdx + i) < _count; i++) {
        int idx = startIdx + i;
        bool selected = (idx == _selectedIdx);
        drawConversationItem(idx, i, selected);
    }
}

void ConversationsScreen::drawConversationItem(int idx, int visibleIdx, bool selected) {
    UnifiedConversation& conv = _conversations[idx];

    int16_t y = Theme::CONTENT_Y + 32 + (visibleIdx * 60);
    int16_t x = 4;
    int16_t w = Theme::SCREEN_WIDTH - 8;
    int16_t h = 56;

    // Draw card background
    uint16_t bgColor = selected ? Theme::COLOR_BG_ELEVATED : Theme::COLOR_BG_CARD;
    Display::drawCard(x, y, w, h, bgColor, Theme::CARD_RADIUS, false);

    if (selected) {
        Display::drawRoundRect(x, y, w, h, Theme::CARD_RADIUS, Theme::COLOR_PRIMARY);
    }

    // Icon (channel hash # or DM person icon)
    int16_t iconX = x + 10;
    int16_t iconY = y + 12;
    const uint8_t* icon = (conv.type == TYPE_CHANNEL) ? Icons::CHANNEL_ICON : Icons::CONTACTS_ICON;
    uint16_t iconColor = selected ? Theme::COLOR_PRIMARY : Theme::COLOR_TEXT_SECONDARY;
    Display::drawBitmap(iconX, iconY, icon, 16, 16, iconColor);

    // Name (channel name or contact name)
    int16_t nameX = iconX + 20;
    int16_t nameY = y + 8;
    const char* name = (conv.type == TYPE_CHANNEL) ? conv.channelName : conv.contactName;
    Display::drawText(nameX, nameY, name, Theme::COLOR_TEXT_PRIMARY, 1);

    // Time ago (right-aligned)
    char timeStr[16];
    uint32_t age = (millis() / 1000) - conv.lastMessageTime;
    if (age < 60) {
        snprintf(timeStr, 16, "%ds", (int)age);
    } else if (age < 3600) {
        snprintf(timeStr, 16, "%dm", (int)(age / 60));
    } else if (age < 86400) {
        snprintf(timeStr, 16, "%dh", (int)(age / 3600));
    } else {
        snprintf(timeStr, 16, "%dd", (int)(age / 86400));
    }
    int16_t timeX = x + w - 30;
    Display::drawText(timeX, nameY, timeStr, Theme::COLOR_TEXT_HINT, 1);

    // Preview text (truncated)
    int16_t previewY = y + 24;
    const char* preview = Theme::truncateText(conv.lastMessagePreview, w - 40, 1);
    Display::drawText(nameX, previewY, preview, Theme::COLOR_TEXT_SECONDARY, 1);

    // Unread badge (if any)
    if (conv.unreadCount > 0) {
        int16_t badgeX = x + w - 16;
        int16_t badgeY = y + h - 16;
        Display::drawBadge(badgeX, badgeY, conv.unreadCount, Theme::COLOR_ERROR);
    }

    // Muted indicator (if muted)
    if (conv.isMuted) {
        // Draw mute icon
        int16_t muteX = x + w - 36;
        int16_t muteY = y + h - 18;
        Display::fillCircle(muteX, muteY, 6, Theme::GRAY_MID);
        // Draw slash through (simplified)
        Display::drawLine(muteX - 4, muteY - 4, muteX + 4, muteY + 4, Theme::WHITE);
    }
}

bool ConversationsScreen::handleInput(const InputData& input) {
    switch (input.event) {
        case InputEvent::TRACKBALL_UP:
            if (_selectedIdx > 0) {
                _selectedIdx--;
                _listView.scrollToItem(_selectedIdx);
                requestRedraw();
            }
            return true;

        case InputEvent::TRACKBALL_DOWN:
            if (_selectedIdx < _count - 1) {
                _selectedIdx++;
                _listView.scrollToItem(_selectedIdx);
                requestRedraw();
            }
            return true;

        case InputEvent::TRACKBALL_CLICK:
        case InputEvent::SOFTKEY_CENTER:
            // Open selected conversation
            if (_selectedIdx >= 0 && _selectedIdx < _count) {
                openConversation(_selectedIdx);
            }
            return true;

        case InputEvent::SOFTKEY_LEFT:
            // New message/conversation (TODO)
            return true;

        case InputEvent::BACK:
        case InputEvent::SOFTKEY_RIGHT:
            Screens.goBack();
            return true;

        case InputEvent::TOUCH_TAP:
            // Map touch to conversation item
            {
                int16_t ty = input.touchY;
                int16_t contentY = Theme::CONTENT_Y + 32;

                if (ty >= contentY && ty < contentY + (_count * 60)) {
                    int touchIdx = (ty - contentY) / 60;
                    if (touchIdx >= 0 && touchIdx < _count) {
                        _selectedIdx = touchIdx;
                        openConversation(_selectedIdx);
                        return true;
                    }
                }
            }
            return true;

        default:
            return false;
    }
}

void ConversationsScreen::openConversation(int idx) {
    if (idx < 0 || idx >= _count) return;

    UnifiedConversation& conv = _conversations[idx];

    // TODO: Navigate to MessageThreadScreen with appropriate context
    // For now, just log
    if (conv.type == TYPE_CHANNEL) {
        Serial.printf("[CONV] Open channel %d: %s\n", conv.channelIdx, conv.channelName);
        // Screens.navigateTo(ScreenId::CHAT, conv.channelIdx);
    } else {
        Serial.printf("[CONV] Open DM with %08X: %s\n", conv.contactId, conv.contactName);
        // Screens.navigateTo(ScreenId::MESSAGE_THREAD, conv.contactId);
    }
}

void ConversationsScreen::onChannelMessage(int channelIdx, const char* senderAndText,
                                           uint32_t timestamp, uint8_t hops) {
    // Update conversation list if screen is active
    if (_instance) {
        _instance->buildConversationList();
        _instance->requestRedraw();
    }
}

void ConversationsScreen::onDMReceived(uint32_t senderId, const char* senderName,
                                      const char* text, uint32_t timestamp) {
    // Update conversation list if screen is active
    if (_instance) {
        _instance->buildConversationList();
        _instance->requestRedraw();
    }
}

ConversationsScreen* ConversationsScreen::getInstance() {
    return _instance;
}
