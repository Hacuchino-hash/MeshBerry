/**
 * MeshBerry Audio Driver
 *
 * Interface for audio output (MAX98357A) and input (ES7210)
 *
 * Speaker: MAX98357A I2S Class-D amplifier
 * Microphones: ES7210 ADC with dual MEMS mics
 *
 * See: dev-docs/REQUIREMENTS.md for specifications
 */

#ifndef MESHBERRY_AUDIO_H
#define MESHBERRY_AUDIO_H

#include <Arduino.h>
#include "../config.h"
#include "../settings/DeviceSettings.h"  // For AlertTone enum

// =============================================================================
// AUDIO SETTINGS
// =============================================================================

typedef enum {
    AUDIO_SAMPLE_8K = 8000,
    AUDIO_SAMPLE_16K = 16000,
    AUDIO_SAMPLE_22K = 22050,
    AUDIO_SAMPLE_44K = 44100
} AudioSampleRate_t;

// =============================================================================
// AUDIO DRIVER INTERFACE
// =============================================================================

namespace Audio {

// -------------------------------------------------------------------------
// SPEAKER OUTPUT (MAX98357A)
// -------------------------------------------------------------------------

/**
 * Initialize speaker output
 * @return true if successful
 */
bool initSpeaker();

/**
 * Play a tone
 * @param frequency Frequency in Hz
 * @param duration Duration in milliseconds
 * @param volume Volume 0-100
 */
void playTone(uint16_t frequency, uint16_t duration, uint8_t volume = 100);

/**
 * Play a notification sound
 * Built-in sound effects for alerts
 */
typedef enum {
    SOUND_MESSAGE,      // New message received
    SOUND_SENT,         // Message sent
    SOUND_ERROR,        // Error occurred
    SOUND_CONNECT,      // Node connected
    SOUND_DISCONNECT,   // Node disconnected
    SOUND_KEYPRESS      // Key pressed (optional)
} SoundEffect_t;

void playSound(SoundEffect_t sound);

/**
 * Play low battery warning alert
 * Three descending tones to indicate low battery
 */
void playLowBatteryAlert();

/**
 * Play charging connected tone
 * Ascending "power up" sound when charger is plugged in
 */
void playChargingConnected();

/**
 * Play charging complete tone
 * Triumphant sound when battery is fully charged
 */
void playChargingComplete();

/**
 * Play a configurable alert tone
 * @param tone AlertTone enum value from DeviceSettings
 *             TONE_NONE = silent (no sound)
 */
void playAlertTone(AlertTone tone);

/**
 * Play audio from buffer
 * @param samples PCM audio samples (16-bit signed)
 * @param length Number of samples
 * @param sampleRate Sample rate
 */
void play(const int16_t* samples, size_t length, AudioSampleRate_t sampleRate = AUDIO_SAMPLE_16K);

/**
 * Play audio from SD card
 * @param filename Path to WAV file on SD card
 * @return true if playback started
 */
bool playFile(const char* filename);

/**
 * Stop current playback
 */
void stop();

/**
 * Check if audio is currently playing
 */
bool isPlaying();

/**
 * Set master volume
 * @param volume 0-100
 */
void setVolume(uint8_t volume);

/**
 * Get current volume
 */
uint8_t getVolume();

/**
 * Mute/unmute speaker
 */
void mute();
void unmute();
bool isMuted();

// -------------------------------------------------------------------------
// MICROPHONE INPUT (ES7210) - Future use
// -------------------------------------------------------------------------

/**
 * Initialize microphone input
 * @return true if successful
 */
bool initMicrophone();

/**
 * Start recording audio
 * @param buffer Buffer to store samples
 * @param maxSamples Maximum samples to record
 * @param sampleRate Sample rate
 */
void startRecording(int16_t* buffer, size_t maxSamples, AudioSampleRate_t sampleRate = AUDIO_SAMPLE_16K);

/**
 * Stop recording
 * @return Number of samples recorded
 */
size_t stopRecording();

/**
 * Check if currently recording
 */
bool isRecording();

/**
 * Set microphone gain
 * @param gain Gain level 0-100
 */
void setMicGain(uint8_t gain);

} // namespace Audio

#endif // MESHBERRY_AUDIO_H
