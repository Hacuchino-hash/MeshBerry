/**
 * MeshBerry Settings Manager Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 */

#include "SettingsManager.h"
#include "../crypto/ChannelCrypto.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

// Settings file paths
static const char* SETTINGS_FILE = "/settings.json";
static const char* CHANNELS_FILE = "/channels.json";
static const char* CONTACTS_FILE = "/contacts.json";
static const char* IDENTITY_FILE = "/identity.bin";
static const char* DMS_FILE = "/dms.bin";
static const char* DEVICE_FILE = "/device.json";

// Current settings
static RadioSettings radioSettings;
static ChannelSettings channelSettings;
static ContactSettings contactSettings;
static DMSettings dmSettings;
static DeviceSettings deviceSettings;
static bool initialized = false;

namespace SettingsManager {

// Forward declarations for channel loading/saving
static bool loadChannels();
static bool saveChannels();
static bool loadContacts();
static bool saveContactsInternal();
static bool loadDMs();
static bool saveDMsInternal();
static bool loadDevice();
static bool saveDeviceInternal();

bool init() {
    if (initialized) return true;

    // Load settings or apply defaults
    if (!load()) {
        Serial.println("[SETTINGS] No valid settings found, using defaults");
        resetToDefaults();
        save();  // Save defaults
    }

    // Load channels separately
    if (!loadChannels()) {
        Serial.println("[SETTINGS] No valid channels found, using defaults");
        channelSettings.setDefaults();
        saveChannels();
    }

    // Load contacts separately
    if (!loadContacts()) {
        Serial.println("[SETTINGS] No valid contacts found, using defaults");
        contactSettings.setDefaults();
        saveContactsInternal();
    }

    // Load DM conversations separately
    if (!loadDMs()) {
        Serial.println("[SETTINGS] No valid DMs found, using defaults");
        dmSettings.setDefaults();
        // Don't save yet - wait until there's actual data
    }

    // Load device settings separately
    if (!loadDevice()) {
        Serial.println("[SETTINGS] No valid device settings found, using defaults");
        deviceSettings.setDefaults();
        saveDeviceInternal();
    }

    initialized = true;
    return true;
}

bool load() {
    if (!SPIFFS.exists(SETTINGS_FILE)) {
        Serial.println("[SETTINGS] Settings file not found");
        return false;
    }

    File file = SPIFFS.open(SETTINGS_FILE, "r");
    if (!file) {
        Serial.println("[SETTINGS] Failed to open settings file");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("[SETTINGS] JSON parse error: %s\n", error.c_str());
        return false;
    }

    // Check magic
    uint32_t magic = doc["magic"] | 0;
    if (magic != RadioSettings::SETTINGS_MAGIC) {
        Serial.println("[SETTINGS] Invalid magic number");
        return false;
    }

    // Load radio settings
    radioSettings.magic = magic;
    radioSettings.region = (LoRaRegion)(doc["region"] | 0);
    radioSettings.frequency = doc["frequency"] | 915.0f;
    radioSettings.bandwidth = doc["bandwidth"] | 250.0f;
    radioSettings.spreadingFactor = doc["sf"] | 10;
    radioSettings.codingRate = doc["cr"] | 5;
    radioSettings.txPower = doc["txPower"] | 22;

    // Validate
    if (!radioSettings.isValid()) {
        Serial.println("[SETTINGS] Loaded settings invalid, resetting");
        return false;
    }

    Serial.printf("[SETTINGS] Loaded: Region=%s, Freq=%.3f MHz, SF=%d, BW=%s kHz, TX=%d dBm\n",
                  radioSettings.getRegionName(),
                  radioSettings.frequency,
                  radioSettings.spreadingFactor,
                  radioSettings.getBandwidthStr(),
                  radioSettings.txPower);

    return true;
}

bool save() {
    File file = SPIFFS.open(SETTINGS_FILE, "w");
    if (!file) {
        Serial.println("[SETTINGS] Failed to create settings file");
        return false;
    }

    JsonDocument doc;

    // Save radio settings
    doc["magic"] = radioSettings.magic;
    doc["region"] = (uint8_t)radioSettings.region;
    doc["frequency"] = radioSettings.frequency;
    doc["bandwidth"] = radioSettings.bandwidth;
    doc["sf"] = radioSettings.spreadingFactor;
    doc["cr"] = radioSettings.codingRate;
    doc["txPower"] = radioSettings.txPower;

    if (serializeJson(doc, file) == 0) {
        Serial.println("[SETTINGS] Failed to write settings");
        file.close();
        return false;
    }

    file.close();
    Serial.println("[SETTINGS] Settings saved");

    // Also save channels
    saveChannels();

    return true;
}

static bool loadChannels() {
    if (!SPIFFS.exists(CHANNELS_FILE)) {
        Serial.println("[SETTINGS] Channels file not found");
        return false;
    }

    File file = SPIFFS.open(CHANNELS_FILE, "r");
    if (!file) {
        Serial.println("[SETTINGS] Failed to open channels file");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("[SETTINGS] Channels JSON parse error: %s\n", error.c_str());
        return false;
    }

    // Check magic
    uint32_t magic = doc["magic"] | 0;
    if (magic != ChannelSettings::CHANNEL_MAGIC) {
        Serial.println("[SETTINGS] Invalid channels magic number");
        return false;
    }

    channelSettings.magic = magic;
    channelSettings.numChannels = doc["numChannels"] | 0;

    // Load each channel
    JsonArray channels = doc["channels"];
    int idx = 0;
    for (JsonObject ch : channels) {
        if (idx >= MAX_CHANNELS) break;

        ChannelEntry& entry = channelSettings.channels[idx];
        strlcpy(entry.name, ch["name"] | "", sizeof(entry.name));
        entry.secretLen = ch["secretLen"] | 0;
        entry.hash = ch["hash"] | 0;
        entry.isHashtag = ch["isHashtag"] | false;
        entry.isActive = ch["isActive"] | false;

        // Decode base64 secret
        const char* secretB64 = ch["secret"] | "";
        Serial.printf("[SETTINGS] Loading channel '%s', secret base64: '%s'\n", entry.name, secretB64);
        if (strlen(secretB64) > 0) {
            int decodedLen = ChannelCrypto::decodePSK(secretB64, entry.secret, sizeof(entry.secret));
            Serial.printf("[SETTINGS] Decoded %d bytes: ", decodedLen);
            for (int i = 0; i < decodedLen && i < 8; i++) Serial.printf("%02X ", entry.secret[i]);
            Serial.println("...");
            if (decodedLen > 0) {
                entry.secretLen = decodedLen;
                // IMPORTANT: Recalculate hash from the decoded secret
                // This ensures the hash is correct even if the stored hash was corrupted
                ChannelCrypto::deriveHash(entry.secret, entry.secretLen, &entry.hash);
                Serial.printf("[SETTINGS] Channel '%s' loaded: secretLen=%d, hash=0x%02X\n",
                              entry.name, entry.secretLen, entry.hash);
            } else {
                Serial.printf("[SETTINGS] WARNING: Failed to decode secret for channel '%s'\n", entry.name);
                entry.isActive = false;  // Mark as inactive if key decode failed
            }
        } else {
            Serial.printf("[SETTINGS] WARNING: Channel '%s' has no secret\n", entry.name);
            entry.isActive = false;  // Mark as inactive if no key
        }

        idx++;
    }

    // Validate
    if (!channelSettings.isValid()) {
        Serial.println("[SETTINGS] Loaded channels invalid, resetting");
        return false;
    }

    Serial.printf("[SETTINGS] Loaded %d channels\n", channelSettings.numChannels);

    return true;
}

static bool saveChannels() {
    File file = SPIFFS.open(CHANNELS_FILE, "w");
    if (!file) {
        Serial.println("[SETTINGS] Failed to create channels file");
        return false;
    }

    JsonDocument doc;

    doc["magic"] = channelSettings.magic;
    doc["numChannels"] = channelSettings.numChannels;

    JsonArray channels = doc["channels"].to<JsonArray>();
    for (int i = 0; i < channelSettings.numChannels; i++) {
        const ChannelEntry& entry = channelSettings.channels[i];
        JsonObject ch = channels.add<JsonObject>();

        ch["name"] = entry.name;
        ch["secretLen"] = entry.secretLen;
        ch["hash"] = entry.hash;
        ch["isHashtag"] = entry.isHashtag;
        ch["isActive"] = entry.isActive;

        // Encode secret as base64
        char secretB64[64];
        ChannelCrypto::encodePSK(entry.secret, entry.secretLen, secretB64);
        ch["secret"] = secretB64;
    }

    if (serializeJson(doc, file) == 0) {
        Serial.println("[SETTINGS] Failed to write channels");
        file.close();
        return false;
    }

    file.close();
    Serial.println("[SETTINGS] Channels saved");
    return true;
}

void resetToDefaults() {
    radioSettings.setDefaults();
    channelSettings.setDefaults();
    contactSettings.setDefaults();
    deviceSettings.setDefaults();
    Serial.println("[SETTINGS] Reset to defaults");
}

RadioSettings& getRadioSettings() {
    return radioSettings;
}

ChannelSettings& getChannelSettings() {
    return channelSettings;
}

ContactSettings& getContactSettings() {
    return contactSettings;
}

DMSettings& getDMSettings() {
    return dmSettings;
}

DeviceSettings& getDeviceSettings() {
    return deviceSettings;
}

bool saveContacts() {
    return saveContactsInternal();
}

bool saveDMs() {
    return saveDMsInternal();
}

bool saveDeviceSettings() {
    return saveDeviceInternal();
}

static bool loadContacts() {
    Serial.println("[SETTINGS] >>> loadContacts() entry");
    Serial.flush();

    if (!SPIFFS.exists(CONTACTS_FILE)) {
        Serial.println("[SETTINGS] Contacts file not found");
        return false;
    }

    Serial.println("[SETTINGS] Contacts file exists, opening...");

    File file = SPIFFS.open(CONTACTS_FILE, "r");
    if (!file) {
        Serial.println("[SETTINGS] Failed to open contacts file");
        return false;
    }

    Serial.printf("[SETTINGS] File opened, size=%d bytes\n", file.size());

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("[SETTINGS] Contacts JSON parse error: %s\n", error.c_str());
        return false;
    }

    Serial.println("[SETTINGS] JSON parsed successfully, checking magic...");

    // Check magic
    uint32_t magic = doc["magic"] | 0;
    if (magic != ContactSettings::CONTACT_MAGIC) {
        Serial.println("[SETTINGS] Invalid contacts magic number");
        return false;
    }

    contactSettings.magic = magic;
    contactSettings.numContacts = doc["numContacts"] | 0;

    // Load each contact
    JsonArray contacts = doc["contacts"];
    int idx = 0;
    for (JsonObject c : contacts) {
        if (idx >= ContactSettings::MAX_CONTACTS) break;

        ContactEntry& entry = contactSettings.contacts[idx];
        entry.id = c["id"] | 0;
        strlcpy(entry.name, c["name"] | "", sizeof(entry.name));
        entry.type = (NodeType)(c["type"] | 0);
        entry.lastRssi = c["lastRssi"] | 0;
        entry.lastSnr = c["lastSnr"] | 0.0f;
        entry.lastHeard = c["lastHeard"] | 0;
        entry.isFavorite = c["isFavorite"] | false;
        entry.isActive = c["isActive"] | true;  // Default to true for loaded contacts

        // Load pubKey (base64 encoded)
        const char* pubKeyB64 = c["pubKey"] | "";
        Serial.printf("[SETTINGS] Raw pubKeyB64 for '%s': '%s' (len=%d)\n", entry.name, pubKeyB64, strlen(pubKeyB64));
        if (strlen(pubKeyB64) > 0) {
            int decodedLen = ChannelCrypto::decodePSK(pubKeyB64, entry.pubKey, sizeof(entry.pubKey));
            Serial.printf("[SETTINGS] Decoded pubKey for '%s': len=%d, bytes=", entry.name, decodedLen);
            for (int i = 0; i < 8; i++) Serial.printf("%02X", entry.pubKey[i]);
            Serial.println("...");

            if (decodedLen != 32) {
                Serial.printf("[SETTINGS] WARNING: pubKey decoded to %d bytes, expected 32, clearing\n", decodedLen);
                memset(entry.pubKey, 0, sizeof(entry.pubKey));
            }
        } else {
            memset(entry.pubKey, 0, sizeof(entry.pubKey));
        }

        // Load saved password
        strlcpy(entry.savedPassword, c["savedPwd"] | "", sizeof(entry.savedPassword));

        idx++;
    }

    // Validate
    if (!contactSettings.isValid()) {
        Serial.println("[SETTINGS] Loaded contacts invalid, resetting");
        return false;
    }

    Serial.printf("[SETTINGS] Loaded %d contacts\n", contactSettings.numContacts);
    return true;
}

static bool saveContactsInternal() {
    Serial.printf("[SETTINGS] >>> saveContactsInternal() - saving %d contacts\n", contactSettings.numContacts);

    // Debug: show what we're about to save
    for (int i = 0; i < contactSettings.numContacts; i++) {
        const ContactEntry& e = contactSettings.contacts[i];
        Serial.printf("[SETTINGS] Contact[%d]: name='%s' pubKey[0..3]=%02X%02X%02X%02X\n",
                      i, e.name, e.pubKey[0], e.pubKey[1], e.pubKey[2], e.pubKey[3]);
    }

    File file = SPIFFS.open(CONTACTS_FILE, "w");
    if (!file) {
        Serial.println("[SETTINGS] Failed to create contacts file");
        return false;
    }

    JsonDocument doc;

    doc["magic"] = contactSettings.magic;
    doc["numContacts"] = contactSettings.numContacts;

    JsonArray contacts = doc["contacts"].to<JsonArray>();
    for (int i = 0; i < contactSettings.numContacts; i++) {
        const ContactEntry& entry = contactSettings.contacts[i];
        JsonObject c = contacts.add<JsonObject>();

        c["id"] = entry.id;
        c["name"] = entry.name;
        c["type"] = (uint8_t)entry.type;
        c["lastRssi"] = entry.lastRssi;
        c["lastSnr"] = entry.lastSnr;
        c["lastHeard"] = entry.lastHeard;
        c["isFavorite"] = entry.isFavorite;
        c["isActive"] = entry.isActive;

        // Save pubKey (base64 encoded)
        char pubKeyB64[64];
        ChannelCrypto::encodePSK(entry.pubKey, 32, pubKeyB64);
        c["pubKey"] = pubKeyB64;

        // Debug: show encoded pubKey
        Serial.printf("[SETTINGS] Encoded pubKey for '%s': '%s'\n", entry.name, pubKeyB64);

        // Save password (only if set)
        if (entry.savedPassword[0] != '\0') {
            c["savedPwd"] = entry.savedPassword;
        }
    }

    size_t bytesWritten = serializeJson(doc, file);
    if (bytesWritten == 0) {
        Serial.println("[SETTINGS] Failed to write contacts");
        file.close();
        return false;
    }

    file.close();
    Serial.printf("[SETTINGS] Contacts saved (%d bytes written)\n", bytesWritten);

    // Verify file was written correctly
    if (SPIFFS.exists(CONTACTS_FILE)) {
        File verify = SPIFFS.open(CONTACTS_FILE, "r");
        if (verify) {
            Serial.printf("[SETTINGS] Verified: contacts.json size=%d bytes\n", verify.size());
            verify.close();
        }
    }

    return true;
}

bool applyRadioSettings() {
    // This will be implemented to call LoRa driver functions
    // For now, just return true - actual application happens in lora.cpp
    Serial.printf("[SETTINGS] Applying: Freq=%.3f MHz, SF=%d, BW=%.0f kHz, CR=%d, TX=%d dBm\n",
                  radioSettings.frequency,
                  radioSettings.spreadingFactor,
                  radioSettings.bandwidth,
                  radioSettings.codingRate,
                  radioSettings.txPower);
    return true;
}

void setRegion(LoRaRegion region) {
    radioSettings.setRegionPreset(region);
    Serial.printf("[SETTINGS] Region set to %s\n", radioSettings.getRegionName());
}

void setFrequency(float mhz) {
    if (mhz < 137.0f || mhz > 1020.0f) return;
    radioSettings.frequency = mhz;
    radioSettings.region = REGION_CUSTOM;
}

void setBandwidth(float bw) {
    if (bw != BW_125 && bw != BW_250 && bw != BW_500) return;
    radioSettings.bandwidth = bw;
    radioSettings.region = REGION_CUSTOM;
}

void setSpreadingFactor(uint8_t sf) {
    if (sf < SF_MIN || sf > SF_MAX) return;
    radioSettings.spreadingFactor = sf;
    radioSettings.region = REGION_CUSTOM;
}

void setCodingRate(uint8_t cr) {
    if (cr < 5 || cr > 8) return;
    radioSettings.codingRate = cr;
    radioSettings.region = REGION_CUSTOM;
}

void setTxPower(uint8_t dbm) {
    if (dbm < TX_POWER_MIN || dbm > TX_POWER_MAX) return;
    radioSettings.txPower = dbm;
    radioSettings.region = REGION_CUSTOM;
}

// =============================================================================
// DM PERSISTENCE (binary format for efficiency)
// =============================================================================

static bool loadDMs() {
    if (!SPIFFS.exists(DMS_FILE)) {
        Serial.println("[SETTINGS] DMs file not found");
        return false;
    }

    File file = SPIFFS.open(DMS_FILE, "r");
    if (!file) {
        Serial.println("[SETTINGS] Failed to open DMs file");
        return false;
    }

    // Read magic first
    uint32_t magic = 0;
    if (file.read((uint8_t*)&magic, 4) != 4 || magic != DMSettings::DM_MAGIC) {
        Serial.println("[SETTINGS] Invalid DMs magic number");
        file.close();
        return false;
    }

    // Read the full struct
    file.seek(0);
    size_t bytesRead = file.read((uint8_t*)&dmSettings, sizeof(DMSettings));
    file.close();

    if (bytesRead != sizeof(DMSettings)) {
        Serial.printf("[SETTINGS] DMs file size mismatch: %d vs %d\n", bytesRead, sizeof(DMSettings));
        return false;
    }

    if (!dmSettings.isValid()) {
        Serial.println("[SETTINGS] Loaded DMs invalid, resetting");
        return false;
    }

    int activeCount = dmSettings.getActiveConversationCount();
    Serial.printf("[SETTINGS] Loaded %d DM conversations\n", activeCount);
    return true;
}

static bool saveDMsInternal() {
    File file = SPIFFS.open(DMS_FILE, "w");
    if (!file) {
        Serial.println("[SETTINGS] Failed to create DMs file");
        return false;
    }

    size_t bytesWritten = file.write((uint8_t*)&dmSettings, sizeof(DMSettings));
    file.close();

    if (bytesWritten != sizeof(DMSettings)) {
        Serial.println("[SETTINGS] Failed to write DMs");
        return false;
    }

    Serial.printf("[SETTINGS] DMs saved (%d bytes)\n", bytesWritten);
    return true;
}

// =============================================================================
// DEVICE SETTINGS PERSISTENCE
// =============================================================================

static bool loadDevice() {
    if (!SPIFFS.exists(DEVICE_FILE)) {
        Serial.println("[SETTINGS] Device file not found");
        return false;
    }

    File file = SPIFFS.open(DEVICE_FILE, "r");
    if (!file) {
        Serial.println("[SETTINGS] Failed to open device file");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("[SETTINGS] Device JSON parse error: %s\n", error.c_str());
        return false;
    }

    // Check magic
    uint32_t magic = doc["magic"] | 0;
    if (magic != DeviceSettings::DEVICE_MAGIC) {
        Serial.println("[SETTINGS] Invalid device magic number");
        return false;
    }

    deviceSettings.magic = magic;

    // GPS settings
    deviceSettings.gpsEnabled = doc["gpsEnabled"] | false;
    deviceSettings.gpsRtcSyncEnabled = doc["gpsRtcSyncEnabled"] | true;

    // Display settings
    deviceSettings.backlightTimeout = doc["backlightTimeout"] | 30;

    // Sleep settings (always enabled, light sleep by default)
    deviceSettings.useDeepSleep = doc["useDeepSleep"] | false;
    deviceSettings.sleepTimeoutSecs = doc["sleepTimeoutSecs"] | 120;
    deviceSettings.sleepDurationSecs = doc["sleepDurationSecs"] | 1800;
    deviceSettings.wakeOnLoRa = doc["wakeOnLoRa"] | true;
    deviceSettings.wakeOnButton = doc["wakeOnButton"] | true;

    // Audio settings
    deviceSettings.audioVolume = doc["audioVolume"] | 80;
    deviceSettings.audioMuted = doc["audioMuted"] | false;
    deviceSettings.toneMessage = (AlertTone)(doc["toneMessage"] | (uint8_t)TONE_BBM_PING);
    deviceSettings.toneNodeConnect = (AlertTone)(doc["toneNodeConnect"] | (uint8_t)TONE_ASCENDING);
    deviceSettings.toneLowBattery = (AlertTone)(doc["toneLowBattery"] | (uint8_t)TONE_WARNING);
    deviceSettings.toneSent = (AlertTone)(doc["toneSent"] | (uint8_t)TONE_CHIRP);
    deviceSettings.toneError = (AlertTone)(doc["toneError"] | (uint8_t)TONE_DESCENDING);

    Serial.printf("[SETTINGS] Device settings loaded: gpsEnabled=%d, gpsRtcSync=%d, deepSleep=%d, vol=%d\n",
                  deviceSettings.gpsEnabled, deviceSettings.gpsRtcSyncEnabled,
                  deviceSettings.useDeepSleep, deviceSettings.audioVolume);
    return true;
}

static bool saveDeviceInternal() {
    File file = SPIFFS.open(DEVICE_FILE, "w");
    if (!file) {
        Serial.println("[SETTINGS] Failed to create device file");
        return false;
    }

    JsonDocument doc;

    doc["magic"] = deviceSettings.magic;

    // GPS settings
    doc["gpsEnabled"] = deviceSettings.gpsEnabled;
    doc["gpsRtcSyncEnabled"] = deviceSettings.gpsRtcSyncEnabled;

    // Display settings
    doc["backlightTimeout"] = deviceSettings.backlightTimeout;

    // Sleep settings
    doc["useDeepSleep"] = deviceSettings.useDeepSleep;
    doc["sleepTimeoutSecs"] = deviceSettings.sleepTimeoutSecs;
    doc["sleepDurationSecs"] = deviceSettings.sleepDurationSecs;
    doc["wakeOnLoRa"] = deviceSettings.wakeOnLoRa;
    doc["wakeOnButton"] = deviceSettings.wakeOnButton;

    // Audio settings
    doc["audioVolume"] = deviceSettings.audioVolume;
    doc["audioMuted"] = deviceSettings.audioMuted;
    doc["toneMessage"] = (uint8_t)deviceSettings.toneMessage;
    doc["toneNodeConnect"] = (uint8_t)deviceSettings.toneNodeConnect;
    doc["toneLowBattery"] = (uint8_t)deviceSettings.toneLowBattery;
    doc["toneSent"] = (uint8_t)deviceSettings.toneSent;
    doc["toneError"] = (uint8_t)deviceSettings.toneError;

    if (serializeJson(doc, file) == 0) {
        Serial.println("[SETTINGS] Failed to write device settings");
        file.close();
        return false;
    }

    file.close();
    Serial.println("[SETTINGS] Device settings saved");
    return true;
}

// =============================================================================
// IDENTITY PERSISTENCE
// =============================================================================

bool loadIdentity(mesh::LocalIdentity& identity) {
    if (!SPIFFS.exists(IDENTITY_FILE)) {
        Serial.println("[SETTINGS] Identity file not found");
        return false;
    }

    File file = SPIFFS.open(IDENTITY_FILE, "r");
    if (!file) {
        Serial.println("[SETTINGS] Failed to open identity file");
        return false;
    }

    // LocalIdentity is 96 bytes (32 pub + 64 prv)
    if (file.size() != 96) {
        Serial.printf("[SETTINGS] Identity file wrong size: %d (expected 96)\n", file.size());
        file.close();
        return false;
    }

    uint8_t buffer[96];
    size_t bytesRead = file.read(buffer, 96);
    file.close();

    if (bytesRead != 96) {
        Serial.println("[SETTINGS] Failed to read identity data");
        return false;
    }

    identity.readFrom(buffer, 96);
    Serial.println("[SETTINGS] Identity loaded from SPIFFS");
    return true;
}

bool saveIdentity(mesh::LocalIdentity& identity) {
    File file = SPIFFS.open(IDENTITY_FILE, "w");
    if (!file) {
        Serial.println("[SETTINGS] Failed to create identity file");
        return false;
    }

    uint8_t buffer[96];
    identity.writeTo(buffer, 96);

    size_t bytesWritten = file.write(buffer, 96);
    file.close();

    if (bytesWritten != 96) {
        Serial.println("[SETTINGS] Failed to write identity data");
        return false;
    }

    Serial.println("[SETTINGS] Identity saved to SPIFFS");
    return true;
}

bool hasIdentity() {
    return SPIFFS.exists(IDENTITY_FILE);
}

} // namespace SettingsManager
