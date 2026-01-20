/**
 * MeshBerry DM Chat Screen
 *
 * Direct message conversation view with a specific contact.
 * Similar to ChatScreen but for peer-to-peer encrypted messages.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#ifndef MESHBERRY_DM_CHAT_SCREEN_H
#define MESHBERRY_DM_CHAT_SCREEN_H

#include "Screen.h"
#include "ScreenManager.h"

class DMChatScreen : public Screen {
public:
    DMChatScreen() = default;
    ~DMChatScreen() override = default;

    ScreenId getId() const override { return ScreenId::DM_CHAT; }
    void onEnter() override;
    void onExit() override;
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    const char* getTitle() const override;
    void configureSoftKeys() override;

    /**
     * Set the contact to chat with
     * Called before navigating to this screen
     */
    void setContact(uint32_t contactId, const char* contactName);

    /**
     * Get current contact ID
     */
    uint32_t getContactId() const { return _contactId; }

    /**
     * Called when a DM is received for this contact (live update)
     */
    void onMessageReceived(const char* text, uint32_t timestamp);

    /**
     * Static helper to add message to the currently viewed DM chat
     * Safe to call even if not on DMChatScreen
     */
    static void addToCurrentChat(uint32_t contactId, const char* text, uint32_t timestamp);

private:
    uint32_t _contactId = 0;
    char _contactName[32];

    // Scroll position in conversation
    int _scrollOffset = 0;

    // Text input buffer
    char _inputBuffer[128];
    int _inputPos = 0;
    bool _inputMode = false;

    // Drawing helpers
    void drawMessages(bool fullRedraw);
    void drawInputBar();
    int getVisibleMessageCount() const;

    // Message handling
    void sendMessage();
    void clearInput();

    // Singleton instance pointer for static callback
    static DMChatScreen* _instance;
};

#endif // MESHBERRY_DM_CHAT_SCREEN_H
