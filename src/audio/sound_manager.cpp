#include "audio/sound_manager.h"
#include "cyd_pins.h"

// Channel 6 uses high-speed timer 3, away from the low channels libraries
// tend to claim first
static const uint8_t SOUND_LEDC_CHANNEL = 6;
static const uint8_t SOUND_LEDC_RESOLUTION_BITS = 10;
static const uint32_t SOUND_LEDC_MAX_DUTY = (1UL << SOUND_LEDC_RESOLUTION_BITS) - 1;
static const uint32_t SOUND_INIT_FREQUENCY_HZ = 2000;

static bool initialized = false;
static bool enabled = true;
static bool buttonBeeps = true;
static uint8_t volumePercent = SOUND_DEFAULT_VOLUME;

static ToneStep steps[SOUND_MAX_STEPS];
static uint8_t stepCount = 0;
static uint8_t stepIndex = 0;
static unsigned long stepStartedMs = 0;
static bool playing = false;

static const ToneStep CLICK_STEPS[] = {{3000, 15}};
static const ToneStep BEEP_STEPS[] = {{2000, 120}};
static const ToneStep COUNTDOWN_STEPS[] = {{800, 60}};
static const ToneStep SUCCESS_STEPS[] = {{1500, 80}, {0, 30}, {2500, 120}};
static const ToneStep ERROR_STEPS[] = {{700, 150}, {0, 40}, {450, 250}};

#define SOUND_STEPS(sequence) sequence, sizeof(sequence) / sizeof(sequence[0])

static void silence()
{
    if (initialized)
        ledcWrite(SOUND_LEDC_CHANNEL, 0);
}

// 50% duty is the loudest square wave; volume scales duty below that
static uint32_t volumeDuty()
{
    return (SOUND_LEDC_MAX_DUTY / 2) * volumePercent / 100;
}

static void startStep()
{
    const ToneStep &step = steps[stepIndex];
    stepStartedMs = millis();

    if (step.frequencyHz == 0 ||
        ledcWriteTone(SOUND_LEDC_CHANNEL, step.frequencyHz) == 0)
    {
        silence();
        return;
    }

    // ledcWriteTone() resets duty to 50%
    ledcWrite(SOUND_LEDC_CHANNEL, volumeDuty());
}

bool soundInit()
{
    if (initialized)
        return true;

    if (ledcSetup(SOUND_LEDC_CHANNEL, SOUND_INIT_FREQUENCY_HZ, SOUND_LEDC_RESOLUTION_BITS) == 0)
    {
        Serial.println("Sound: LEDC setup failed.");
        return false;
    }

    ledcAttachPin(SPEAKER_PIN, SOUND_LEDC_CHANNEL);
    initialized = true;
    silence();
    return true;
}

bool playToneSequence(const ToneStep *sequence, uint8_t count)
{
    if (!initialized || !enabled || volumePercent == 0 ||
        !sequence || count == 0 || count > SOUND_MAX_STEPS)
        return false;

    for (uint8_t i = 0; i < count; ++i)
    {
        if (sequence[i].durationMs == 0)
            return false;
    }

    memcpy(steps, sequence, count * sizeof(ToneStep));
    stepCount = count;
    stepIndex = 0;
    playing = true;
    startStep();
    return true;
}

bool playTone(uint16_t frequencyHz, uint16_t durationMs)
{
    const ToneStep step = {frequencyHz, durationMs};
    return playToneSequence(&step, 1);
}

bool playButtonBeep()
{
    static const uint16_t BUTTON_BEEP_FREQUENCY_HZ = 2500;
    static const uint16_t BUTTON_BEEP_DURATION_MS = 30;

    if (!initialized || !enabled || !buttonBeeps || volumePercent == 0)
        return false;

    stopSound();
    if (ledcWriteTone(SOUND_LEDC_CHANNEL, BUTTON_BEEP_FREQUENCY_HZ) == 0)
    {
        silence();
        return false;
    }
    ledcWrite(SOUND_LEDC_CHANNEL, volumeDuty());
    delay(BUTTON_BEEP_DURATION_MS);
    silence();
    return true;
}

bool playSound(Sound sound)
{
    switch (sound)
    {
    case SOUND_CLICK:     return playToneSequence(SOUND_STEPS(CLICK_STEPS));
    case SOUND_BEEP:      return playToneSequence(SOUND_STEPS(BEEP_STEPS));
    case SOUND_COUNTDOWN: return playToneSequence(SOUND_STEPS(COUNTDOWN_STEPS));
    case SOUND_SUCCESS:   return playToneSequence(SOUND_STEPS(SUCCESS_STEPS));
    case SOUND_ERROR:     return playToneSequence(SOUND_STEPS(ERROR_STEPS));
    }
    return false;
}

void stopSound()
{
    playing = false;
    stepCount = 0;
    stepIndex = 0;
    silence();
}

bool soundPlaying()
{
    return playing;
}

void setSoundEnabled(bool isEnabled)
{
    enabled = isEnabled;
    if (!enabled)
        stopSound();
}

bool soundEnabled()
{
    return enabled;
}

void setButtonBeepsEnabled(bool isEnabled)
{
    buttonBeeps = isEnabled;
}

bool buttonBeepsEnabled()
{
    return buttonBeeps;
}

void setSoundVolume(uint8_t percent)
{
    volumePercent = percent > 100 ? 100 : percent;
    if (volumePercent == 0)
        stopSound();
    else if (playing && steps[stepIndex].frequencyHz != 0)
        ledcWrite(SOUND_LEDC_CHANNEL, volumeDuty());
}

uint8_t soundVolume()
{
    return volumePercent;
}

void soundUpdate()
{
    if (!playing ||
        millis() - stepStartedMs < steps[stepIndex].durationMs)
        return;

    if (++stepIndex >= stepCount)
    {
        stopSound();
        return;
    }

    startStep();
}
