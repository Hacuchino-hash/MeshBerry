/**
 * MeshBerry Audio Driver Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * MeshBerry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MeshBerry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MeshBerry. If not, see <https://www.gnu.org/licenses/>.
 *
 * I2S audio output for MAX98357A amplifier on T-Deck.
 */

#include "audio.h"
#include "../settings/DeviceSettings.h"
#include <driver/i2s.h>
#include <math.h>

// I2S configuration
#define I2S_NUM         I2S_NUM_0
#define I2S_SAMPLE_RATE 16000
#define I2S_BITS        16
#define I2S_CHANNELS    1

// Tone generation
#define TONE_SAMPLE_RATE 16000
// Note: PI is already defined in Arduino.h

// Internal state
static bool speakerInitialized = false;
static bool micInitialized = false;
static bool muted = false;
static uint8_t volume = 80;  // 0-100
static bool currentlyPlaying = false;

// Recording state
static int16_t* recordBuffer = nullptr;
static size_t recordMaxSamples = 0;
static size_t recordedSamples = 0;
static bool recording = false;

namespace Audio {

bool initSpeaker() {
    if (speakerInitialized) return true;

    Serial.println("[AUDIO] Initializing I2S speaker...");

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = PIN_I2S_BCLK,
        .ws_io_num = PIN_I2S_LRCLK,
        .data_out_num = PIN_I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    esp_err_t err = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[AUDIO] Failed to install I2S driver: %d\n", err);
        return false;
    }

    err = i2s_set_pin(I2S_NUM, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[AUDIO] Failed to set I2S pins: %d\n", err);
        i2s_driver_uninstall(I2S_NUM);
        return false;
    }

    i2s_zero_dma_buffer(I2S_NUM);

    speakerInitialized = true;
    Serial.println("[AUDIO] I2S speaker initialized");
    return true;
}

void playTone(uint16_t frequency, uint16_t duration, uint8_t vol) {
    if (!speakerInitialized || muted) return;

    // Use provided volume or default
    uint8_t effectiveVolume = (vol > 0) ? vol : volume;

    // Calculate samples needed
    uint32_t numSamples = (TONE_SAMPLE_RATE * duration) / 1000;

    // Allocate buffer for samples
    int16_t* samples = (int16_t*)malloc(numSamples * sizeof(int16_t));
    if (!samples) {
        Serial.println("[AUDIO] Failed to allocate tone buffer");
        return;
    }

    // Generate sine wave
    float amplitude = (32767.0f * effectiveVolume) / 100.0f;
    float omega = 2.0f * PI * frequency / TONE_SAMPLE_RATE;

    for (uint32_t i = 0; i < numSamples; i++) {
        // Apply envelope to avoid clicks (fade in/out)
        float envelope = 1.0f;
        uint32_t fadeLen = numSamples / 10;  // 10% fade

        if (i < fadeLen) {
            envelope = (float)i / fadeLen;  // Fade in
        } else if (i > numSamples - fadeLen) {
            envelope = (float)(numSamples - i) / fadeLen;  // Fade out
        }

        samples[i] = (int16_t)(amplitude * envelope * sinf(omega * i));
    }

    // Write to I2S
    size_t bytesWritten;
    currentlyPlaying = true;
    i2s_write(I2S_NUM, samples, numSamples * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
    currentlyPlaying = false;

    free(samples);
}

void playSound(SoundEffect_t sound) {
    if (!speakerInitialized || muted) return;

    switch (sound) {
        case SOUND_MESSAGE:
            // BBM-style ping: Two quick ascending tones
            // Classic BBM notification is roughly D6 -> F#6 (very short)
            playTone(1175, 80, volume);   // D6
            delay(30);
            playTone(1480, 120, volume);  // F#6
            break;

        case SOUND_SENT:
            // Quick confirmation chirp - single high tone
            playTone(1568, 60, volume);   // G6
            break;

        case SOUND_ERROR:
            // Two low tones indicating error
            playTone(330, 150, volume);   // E4
            delay(80);
            playTone(262, 200, volume);   // C4
            break;

        case SOUND_CONNECT:
            // Ascending three-tone connection sound
            playTone(523, 80, volume);    // C5
            delay(40);
            playTone(659, 80, volume);    // E5
            delay(40);
            playTone(784, 120, volume);   // G5
            break;

        case SOUND_DISCONNECT:
            // Descending two-tone disconnection
            playTone(784, 100, volume);   // G5
            delay(50);
            playTone(523, 150, volume);   // C5
            break;

        case SOUND_KEYPRESS:
            // Very short subtle click
            playTone(1000, 10, volume / 2);
            break;
    }
}

void playLowBatteryAlert() {
    if (!speakerInitialized || muted) return;

    // Three descending warning tones
    playTone(880, 150, volume);    // A5
    delay(100);
    playTone(698, 150, volume);    // F5
    delay(100);
    playTone(523, 200, volume);    // C5
}

void playChargingConnected() {
    if (!speakerInitialized || muted) return;

    // Happy ascending "power up" sound
    playTone(523, 60, volume);     // C5
    delay(30);
    playTone(659, 60, volume);     // E5
    delay(30);
    playTone(784, 60, volume);     // G5
    delay(30);
    playTone(1047, 100, volume);   // C6
}

void playChargingComplete() {
    if (!speakerInitialized || muted) return;

    // Triumphant two-tone completion sound
    playTone(784, 100, volume);    // G5
    delay(50);
    playTone(1047, 200, volume);   // C6
}

void playAlertTone(AlertTone tone) {
    if (!speakerInitialized || muted) return;
    if (tone == TONE_NONE) return;  // Silent option

    switch (tone) {
        case TONE_BBM_PING:
            // Classic BBM-style ping: D6 -> F#6
            playTone(1175, 80, volume);   // D6
            delay(30);
            playTone(1480, 120, volume);  // F#6
            break;

        case TONE_CHIRP:
            // Quick single chirp
            playTone(1568, 60, volume);   // G6
            break;

        case TONE_DOUBLE_BEEP:
            // Two quick beeps
            playTone(1000, 60, volume);
            delay(40);
            playTone(1000, 60, volume);
            break;

        case TONE_ASCENDING:
            // Ascending three-tone (C-E-G)
            playTone(523, 80, volume);    // C5
            delay(40);
            playTone(659, 80, volume);    // E5
            delay(40);
            playTone(784, 120, volume);   // G5
            break;

        case TONE_DESCENDING:
            // Descending two-tone
            playTone(784, 100, volume);   // G5
            delay(50);
            playTone(523, 150, volume);   // C5
            break;

        case TONE_WARNING:
            // Three descending warning tones
            playTone(880, 150, volume);   // A5
            delay(100);
            playTone(698, 150, volume);   // F5
            delay(100);
            playTone(523, 200, volume);   // C5
            break;

        case TONE_MARIO_COIN:
            // Mario coin sound: B6 -> E7
            playTone(1976, 80, volume);   // B6
            playTone(2637, 200, volume);  // E7
            break;

        default:
            break;
    }
}

void play(const int16_t* samples, size_t length, AudioSampleRate_t sampleRate) {
    if (!speakerInitialized || muted || !samples || length == 0) return;

    // Update sample rate if different
    if (sampleRate != I2S_SAMPLE_RATE) {
        i2s_set_sample_rates(I2S_NUM, sampleRate);
    }

    // Apply volume scaling
    int16_t* scaledSamples = (int16_t*)malloc(length * sizeof(int16_t));
    if (scaledSamples) {
        float scale = volume / 100.0f;
        for (size_t i = 0; i < length; i++) {
            scaledSamples[i] = (int16_t)(samples[i] * scale);
        }

        size_t bytesWritten;
        currentlyPlaying = true;
        i2s_write(I2S_NUM, scaledSamples, length * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
        currentlyPlaying = false;

        free(scaledSamples);
    }

    // Restore default sample rate
    if (sampleRate != I2S_SAMPLE_RATE) {
        i2s_set_sample_rates(I2S_NUM, I2S_SAMPLE_RATE);
    }
}

bool playFile(const char* filename) {
    // TODO: Implement WAV file playback from SD card
    // This will be implemented in Phase 2 with wavplayer.cpp
    Serial.printf("[AUDIO] playFile not yet implemented: %s\n", filename);
    return false;
}

void stop() {
    if (!speakerInitialized) return;

    i2s_zero_dma_buffer(I2S_NUM);
    currentlyPlaying = false;
}

bool isPlaying() {
    return currentlyPlaying;
}

void setVolume(uint8_t vol) {
    volume = (vol > 100) ? 100 : vol;
    Serial.printf("[AUDIO] Volume set to %d%%\n", volume);
}

uint8_t getVolume() {
    return volume;
}

void mute() {
    muted = true;
    Serial.println("[AUDIO] Muted");
}

void unmute() {
    muted = false;
    Serial.println("[AUDIO] Unmuted");
}

bool isMuted() {
    return muted;
}

// -------------------------------------------------------------------------
// MICROPHONE INPUT (ES7210) - Future implementation
// -------------------------------------------------------------------------

bool initMicrophone() {
    // TODO: Implement ES7210 ADC initialization
    Serial.println("[AUDIO] Microphone init not yet implemented");
    return false;
}

void startRecording(int16_t* buffer, size_t maxSamples, AudioSampleRate_t sampleRate) {
    // TODO: Implement recording
    Serial.println("[AUDIO] Recording not yet implemented");
}

size_t stopRecording() {
    recording = false;
    return recordedSamples;
}

bool isRecording() {
    return recording;
}

void setMicGain(uint8_t gain) {
    // TODO: Implement mic gain control via ES7210 registers
}

} // namespace Audio
