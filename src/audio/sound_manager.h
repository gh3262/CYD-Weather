#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H

#include <Arduino.h>

// ====================================================
// Sound manager: non-blocking tones on the CYD speaker
// amplifier (SPEAKER_PIN). Any screen can request a
// sound; soundUpdate() advances it from loop().
// ====================================================

enum Sound
{
    SOUND_CLICK,       // short key click
    SOUND_BEEP,        // general notification beep
    SOUND_COUNTDOWN,   // countdown tick
    SOUND_SUCCESS,     // rising two-tone
    SOUND_ERROR        // low falling two-tone
};

// A step in a sound; frequencyHz 0 is a rest
struct ToneStep
{
    uint16_t frequencyHz;
    uint16_t durationMs;
};

const uint8_t SOUND_MAX_STEPS = 16;
const uint8_t SOUND_DEFAULT_VOLUME = 50;   // percent

// Call once from setup(); returns false if the PWM channel cannot start
bool soundInit();

// Starting a sound replaces the one playing. Returns false when sound is
// disabled, muted, uninitialized, or the request is invalid.
bool playSound(Sound sound);
bool playTone(uint16_t frequencyHz, uint16_t durationMs);
bool playToneSequence(const ToneStep *steps, uint8_t count);
// Short touch-feedback beep. Blocks for its few milliseconds so a following
// screen redraw cannot stretch it; replaces any sound that is playing.
bool playButtonBeep();
// Mutes only button beeps; other sounds still play. On at startup.
void setButtonBeepsEnabled(bool enabled);
bool buttonBeepsEnabled();
void stopSound();
bool soundPlaying();

void setSoundEnabled(bool enabled);
bool soundEnabled();
void setSoundVolume(uint8_t percent);   // 0-100
uint8_t soundVolume();

// Call from loop()
void soundUpdate();

#endif
