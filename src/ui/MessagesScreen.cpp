/**
 * MeshBerry Messages Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "MessagesScreen.h"
#include "ChatScreen.h"
#include "SoftKeyBar.h"
#include "Icons.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../settings/SettingsManager.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// External ChatScreen instance for navigation
extern ChatScreen chatScreen;

// Static member initialization
MessagesScreen::Conversation MessagesScreen::_conversations[MAX_CONVERSATIONS];
int MessagesScreen::_conversationCount = 0;

MessagesScreen::MessagesScreen() {
    _listView.setBounds(0, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT - 30);
    _listView.setItemHeight(48);
}

void MessagesScreen::onEnter() {
    buildConversationList();
    requestRedraw();
}

void MessagesScreen::configureSoftKeys() {
    SoftKeyBar::setLabels(nullptr, "Open", "Back");
}

void MessagesScreen::formatTimeAgo(uint32_t timestamp, char* buf, size_t bufSize) {
    if (timestamp == 0) {
        snprintf(buf, bufSize, "");
        return;
    }

    uint32_t now = millis() / 1000;  // Convert to seconds
    if (timestamp > now) {
        // Timestamp is in the future (clock sync issue) or unix timestamp
        // Treat as "just now"
        snprintf(buf, bufSize, "now");
        return;
    }

    uint32_t diff = now - timestamp;

    if (diff < 60) {
        snprintf(buf, bufSize, "%ds", (int)diff);
    } else if (diff < 3600) {
        snprintf(buf, bufSize, "%dm", (int)(diff / 60));
    } else if (diff < 86400) {
        snprintf(buf, bufSize, "%dh", (int)(diff / 3600));
    } else {
        snprintf(buf, bufSize, "%dd", (int)(diff / 86400));
    }
}

void MessagesScreen::buildConversationList() {
    ChannelSettings& channels = SettingsManager::getChannelSettings();
    _conversationCount = 0;

    // Add each active channel as a conversation
    for (int i = 0; i < channels.numChannels && _conversationCount < MAX_CONVERSATIONS - 1; i++) {
        const ChannelEntry& ch = channels.channels[i];
        if (!ch.isActive) continue;

        Conversation& conv = _conversations[_conversationCount];
        strncpy(conv.name, ch.name, sizeof(conv.name) - 1);
        conv.name[sizeof(conv.name) - 1] = '\0';
        conv.channelIdx = i;

        // Copy name to display string
        strncpy(_primaryStrings[_conversationCount], conv.name, sizeof(_primaryStrings[0]) - 1);
        _primaryStrings[_conversationCount][sizeof(_primaryStrings[0]) - 1] = '\0';

        // Build secondary string with more space for preview
        if (conv.lastTimestamp > 0) {
            char timeStr[12];
            formatTimeAgo(conv.lastTimestamp, timeStr, sizeof(timeStr));

            // Use full secondary string buffer for preview
            snprintf(_secondaryStrings[_conversationCount], sizeof(_secondaryStrings[0]),
                     "%s | %s", timeStr, conv.lastMessage);
        } else {
            strncpy(_secondaryStrings[_conversationCount], "No messages yet",
                    sizeof(_secondaryStrings[0]) - 1);
        }
        _secondaryStrings[_conversationCount][sizeof(_secondaryStrings[0]) - 1] = '\0';

        // Build list item - all channels shown equally
        _listItems[_conversationCount] = {
            _primaryStrings[_conversationCount],
            _secondaryStrings[_conversationCount],
            Icons::CHANNEL_ICON,
            Theme::ACCENT,
            conv.unreadCount > 0,
            Theme::RED,
            (void*)(intptr_t)_conversationCount
        };

        _conversationCount++;
    }

    // Add "Manage Channels" item at the end
    if (_conversationCount < MAX_CONVERSATIONS) {
        Conversation& mgmt = _conversations[_conversationCount];
        strcpy(mgmt.name, "Manage Channels");
        mgmt.channelIdx = -1;  // Special marker
        mgmt.lastTimestamp = 0;
        mgmt.unreadCount = 0;

        strcpy(_primaryStrings[_conversationCount], "+ Manage Channels");
        strcpy(_secondaryStrings[_conversationCount], "Add or delete channels");

        _listItems[_conversationCount] = {
            _primaryStrings[_conversationCount],
            _secondaryStrings[_conversationCount],
            Icons::SETTINGS_ICON,
            Theme::GRAY_LIGHT,
            false,
            0,
            (void*)(intptr_t)_conversationCount
        };

        _conversationCount++;
    }

    _listView.setItems(_listItems, _conversationCount);
}

void MessagesScreen::draw(bool fullRedraw) {
    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Draw title
        Display::drawText(12, Theme::CONTENT_Y + 4, "Messages", Theme::ACCENT, 2);

        // Unread count badge
        int totalUnread = getUnreadCount();
        if (totalUnread > 0) {
            char badge[8];
            snprintf(badge, sizeof(badge), "%d", totalUnread);
            int16_t badgeW = strlen(badge) * 6 + 8;
            int16_t badgeX = Theme::SCREEN_WIDTH - badgeW - 8;
            Display::fillRoundRect(badgeX, Theme::CONTENT_Y + 4, badgeW, 16, 4, Theme::RED);
            Display::drawText(badgeX + 4, Theme::CONTENT_Y + 8, badge, Theme::WHITE, 1);
        }

        // Divider below title
        Display::drawHLine(12, Theme::CONTENT_Y + 26, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);
    }

    if (_conversationCount == 0) {
        Display::drawTextCentered(0, Theme::CONTENT_Y + 80,
                                  Theme::SCREEN_WIDTH,
                                  "No channels", Theme::TEXT_SECONDARY, 1);
    } else {
        _listView.draw(fullRedraw);
    }
}

bool MessagesScreen::handleInput(const InputData& input) {
    // Center soft key: "Open" - open selected conversation
    if (input.event == InputEvent::SOFTKEY_CENTER ||
        input.event == InputEvent::TRACKBALL_CLICK) {
        if (_conversationCount > 0) {
            onConversationSelected(_listView.getSelectedIndex());
        }
        return true;
    }

    // Right soft key or Back: go back
    // Treat backspace as back since this screen has no text input
    bool isBackKey = (input.event == InputEvent::KEY_PRESS && input.keyCode == KEY_BACKSPACE);
    if (input.event == InputEvent::SOFTKEY_RIGHT ||
        input.event == InputEvent::BACK ||
        input.event == InputEvent::TRACKBALL_LEFT ||
        isBackKey) {
        Screens.goBack();
        return true;
    }

    // Let list view handle up/down
    if (_listView.handleTrackball(
            input.event == InputEvent::TRACKBALL_UP,
            input.event == InputEvent::TRACKBALL_DOWN,
            false, false,
            false)) {
        requestRedraw();
        return true;
    }

    // Handle touch tap
    if (input.event == InputEvent::TOUCH_TAP) {
        int16_t ty = input.touchY;
        int16_t tx = input.touchX;

        // Soft key bar touch (Y >= 210)
        if (ty >= Theme::SOFTKEY_BAR_Y) {
            if (tx >= 214) {
                // Right soft key = Back
                Screens.goBack();
            } else if (tx >= 107) {
                // Center soft key = Open
                if (_conversationCount > 0) {
                    onConversationSelected(_listView.getSelectedIndex());
                }
            }
            // Left soft key has no action on this screen
            return true;
        }

        // List touch - just select, don't open (use soft key or trackball click to open)
        int16_t listTop = Theme::CONTENT_Y + 30;
        if (ty >= listTop && _conversationCount > 0) {
            int itemIndex = _listView.getScrollOffset() + (ty - listTop) / 48;
            if (itemIndex >= 0 && itemIndex < _conversationCount) {
                _listView.setSelectedIndex(itemIndex);
                requestRedraw();
                // Don't auto-open - user taps center soft key or trackball click
            }
        }
        return true;
    }

    return false;
}

void MessagesScreen::onConversationSelected(int index) {
    if (index < 0 || index >= _conversationCount) return;

    const Conversation& conv = _conversations[index];

    if (conv.channelIdx == -1) {
        // "Manage Channels" selected - go to ChannelsScreen
        Screens.navigateTo(ScreenId::CHANNELS);
    } else {
        // Channel selected - go to ChatScreen
        chatScreen.setChannel(conv.channelIdx);
        Screens.navigateTo(ScreenId::CHAT);

        // Clear unread count for this channel
        _conversations[index].unreadCount = 0;

        // Update status bar notification badge
        int unread = getUnreadCount();
        Screens.setNotificationCount(unread);
    }
}

void MessagesScreen::onChannelMessage(int channelIdx, const char* senderAndText, uint32_t timestamp, uint8_t hops) {
    // Find the conversation for this channel
    for (int i = 0; i < _conversationCount; i++) {
        if (_conversations[i].channelIdx == channelIdx) {
            // Update last message preview
            strncpy(_conversations[i].lastMessage, senderAndText, sizeof(_conversations[i].lastMessage) - 1);
            _conversations[i].lastMessage[sizeof(_conversations[i].lastMessage) - 1] = '\0';
            _conversations[i].lastTimestamp = timestamp;
            _conversations[i].unreadCount++;

            // Forward to ChatScreen with hop info if it's showing this channel
            ChatScreen::addToCurrentChat(channelIdx, senderAndText, timestamp, hops);
            return;
        }
    }

    // Channel not found in conversation list yet
    // This can happen if a message arrives for a newly added channel
    // The conversation list will be rebuilt on next onEnter()
}

int MessagesScreen::getUnreadCount() {
    int total = 0;
    for (int i = 0; i < _conversationCount; i++) {
        if (_conversations[i].channelIdx >= 0) {
            total += _conversations[i].unreadCount;
        }
    }
    return total;
}
