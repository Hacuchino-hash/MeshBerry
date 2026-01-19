/**
 * MeshBerry Channel Cryptography
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * Key derivation and encoding helpers for channel encryption.
 * Uses SHA256 for key derivation (compatible with MeshCore).
 */

#ifndef MESHBERRY_CHANNEL_CRYPTO_H
#define MESHBERRY_CHANNEL_CRYPTO_H

#include <Arduino.h>

namespace ChannelCrypto {

/**
 * Derive a 32-byte encryption key from a channel name using SHA256.
 * This allows #channelname syntax where the key is auto-generated.
 *
 * @param name Channel name (e.g., "#local", "#emergency")
 * @param key32 Output buffer for 32-byte key
 */
void deriveKeyFromName(const char* name, uint8_t* key32);

/**
 * Derive a 1-byte channel hash from the secret key.
 * Used for efficient packet-to-channel matching.
 *
 * @param secret The encryption key
 * @param secretLen Length of the key (16 or 32 bytes)
 * @param hash Output for 1-byte hash
 */
void deriveHash(const uint8_t* secret, size_t secretLen, uint8_t* hash);

/**
 * Decode a base64-encoded PSK to raw bytes.
 *
 * @param psk_base64 Base64-encoded pre-shared key
 * @param key Output buffer for decoded key
 * @param maxLen Maximum output length
 * @return Number of bytes decoded, or 0 on error
 */
int decodePSK(const char* psk_base64, uint8_t* key, size_t maxLen);

/**
 * Encode a raw key to base64 PSK string.
 *
 * @param key Raw key bytes
 * @param keyLen Length of key
 * @param psk_base64 Output buffer (must be at least (keyLen * 4 / 3) + 4 bytes)
 */
void encodePSK(const uint8_t* key, size_t keyLen, char* psk_base64);

/**
 * Convert bytes to hex string for display.
 *
 * @param dest Output buffer (must be 2*len + 1 bytes)
 * @param src Input bytes
 * @param len Number of bytes
 */
void toHex(char* dest, const uint8_t* src, size_t len);

} // namespace ChannelCrypto

#endif // MESHBERRY_CHANNEL_CRYPTO_H
