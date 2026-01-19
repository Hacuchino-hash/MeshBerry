/**
 * MeshBerry Channel Cryptography Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 */

#include "ChannelCrypto.h"
#include <SHA256.h>

// Use Arduino's built-in base64 functions or simple manual implementation
// to avoid multiple definition issues with header-only base64.hpp

// Base64 encoding table
static const char b64_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Base64 decoding table
static const uint8_t b64_decode_table[128] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 62,  0,  0,  0, 63,
   52, 53, 54, 55, 56, 57, 58, 59, 60, 61,  0,  0,  0,  0,  0,  0,
    0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
   15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,  0,  0,  0,  0,
    0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
   41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,  0,  0,  0,  0,  0
};

static size_t base64_encode_local(const uint8_t* input, size_t inputLen, char* output) {
    size_t i = 0, j = 0;
    uint8_t arr3[3], arr4[4];

    while (inputLen--) {
        arr3[i++] = *(input++);
        if (i == 3) {
            arr4[0] = (arr3[0] & 0xfc) >> 2;
            arr4[1] = ((arr3[0] & 0x03) << 4) + ((arr3[1] & 0xf0) >> 4);
            arr4[2] = ((arr3[1] & 0x0f) << 2) + ((arr3[2] & 0xc0) >> 6);
            arr4[3] = arr3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                output[j++] = b64_alphabet[arr4[i]];
            i = 0;
        }
    }

    if (i) {
        for (size_t k = i; k < 3; k++)
            arr3[k] = 0;

        arr4[0] = (arr3[0] & 0xfc) >> 2;
        arr4[1] = ((arr3[0] & 0x03) << 4) + ((arr3[1] & 0xf0) >> 4);
        arr4[2] = ((arr3[1] & 0x0f) << 2) + ((arr3[2] & 0xc0) >> 6);

        for (size_t k = 0; k < i + 1; k++)
            output[j++] = b64_alphabet[arr4[k]];

        while (i++ < 3)
            output[j++] = '=';
    }

    output[j] = '\0';
    return j;
}

static size_t base64_decode_local(const char* input, size_t inputLen, uint8_t* output) {
    size_t i = 0, j = 0;
    uint8_t arr4[4], arr3[3];

    // Skip padding for length calculation
    while (inputLen > 0 && input[inputLen - 1] == '=')
        inputLen--;

    size_t idx = 0;
    while (idx < inputLen) {
        char c = input[idx++];
        if (c < 0 || c > 127) continue;
        arr4[i++] = b64_decode_table[(int)c];

        if (i == 4) {
            arr3[0] = (arr4[0] << 2) + ((arr4[1] & 0x30) >> 4);
            arr3[1] = ((arr4[1] & 0x0f) << 4) + ((arr4[2] & 0x3c) >> 2);
            arr3[2] = ((arr4[2] & 0x03) << 6) + arr4[3];

            for (i = 0; i < 3; i++)
                output[j++] = arr3[i];
            i = 0;
        }
    }

    if (i) {
        for (size_t k = i; k < 4; k++)
            arr4[k] = 0;

        arr3[0] = (arr4[0] << 2) + ((arr4[1] & 0x30) >> 4);
        arr3[1] = ((arr4[1] & 0x0f) << 4) + ((arr4[2] & 0x3c) >> 2);

        for (size_t k = 0; k < i - 1; k++)
            output[j++] = arr3[k];
    }

    return j;
}

namespace ChannelCrypto {

void deriveKeyFromName(const char* name, uint8_t* key) {
    SHA256 sha;
    sha.update((const uint8_t*)name, strlen(name));
    sha.finalize(key, 16);  // MeshCore uses first 16 bytes of SHA256 for hashtag channels
}

void deriveHash(const uint8_t* secret, size_t secretLen, uint8_t* hash) {
    uint8_t fullHash[32];
    SHA256 sha;
    sha.update(secret, secretLen);
    sha.finalize(fullHash, 32);

    // Use first byte as channel hash (matches MeshCore)
    hash[0] = fullHash[0];
}

int decodePSK(const char* psk_base64, uint8_t* key, size_t maxLen) {
    if (!psk_base64 || !key) return 0;

    size_t inputLen = strlen(psk_base64);
    if (inputLen == 0) return 0;

    // Calculate expected output length accounting for padding
    // Count padding characters
    size_t padding = 0;
    if (inputLen >= 1 && psk_base64[inputLen - 1] == '=') padding++;
    if (inputLen >= 2 && psk_base64[inputLen - 2] == '=') padding++;

    // Actual decoded length = (inputLen * 3 / 4) - padding
    size_t expectedLen = (inputLen * 3) / 4 - padding;
    if (expectedLen > maxLen) {
        Serial.printf("[CRYPTO] decodePSK: expectedLen=%d > maxLen=%d, rejecting\n", expectedLen, maxLen);
        return 0;
    }

    // Decode using local function
    size_t decodedLen = base64_decode_local(psk_base64, inputLen, key);

    // Return decoded length (caller validates if needed)
    // Valid lengths: 16 (channel PSK), 32 (channel PSK or pubKey)
    return (int)decodedLen;
}

void encodePSK(const uint8_t* key, size_t keyLen, char* psk_base64) {
    if (!key || !psk_base64 || keyLen == 0) {
        if (psk_base64) psk_base64[0] = '\0';
        return;
    }

    base64_encode_local(key, keyLen, psk_base64);
}

static const char hex_chars[] = "0123456789ABCDEF";

void toHex(char* dest, const uint8_t* src, size_t len) {
    while (len > 0) {
        uint8_t b = *src++;
        *dest++ = hex_chars[b >> 4];
        *dest++ = hex_chars[b & 0x0F];
        len--;
    }
    *dest = '\0';
}

} // namespace ChannelCrypto
