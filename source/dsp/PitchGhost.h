#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class PitchGhost
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer, float blendFactor);

    void setPossession    (float amount);       // 0.0–1.0, drift intensity
    void setDriftRate     (float rateHz);       // how fast ghost detunes
    void setDriftDirection(int direction);      // -1=down, 0=wander, 1=up
    void setGhostDecay    (float decayMs);

    // Feed audio continuously so ring buffer stays current
    void pushAudio (const juce::AudioBuffer<float>& buffer);

    // Call when envelope crosses threshold downward — reads from ring buffer
    void triggerCapture();

private:
    void  updateDrift  (float deltaTime);
    float pitchToRatio (float cents) const;

    // Ring buffer: continuously accumulates ~1.5s of pre-threshold audio
    static constexpr int kRingSize          = 65536;  // power-of-2 for fast modulo
    juce::AudioBuffer<float> ringBuffer;
    int  ringWritePos = 0;

    // Capture buffer (snapshot copied from ring on trigger)
    static constexpr int kMaxCaptureSamples = 24000;  // up to ~500ms at 48kHz
    juce::AudioBuffer<float> captureBuffer;
    int   captureLength  = 2048;
    float captureReadPos = 0.f;

    bool  ghostActive    = false;

    // Pitch drift state
    float currentPitchCents = 0.f;   // current detuning in cents
    float targetPitchCents  = 0.f;
    float possession        = 0.5f;
    float driftRate         = 0.1f;  // Hz
    int   driftDir          = 0;     // -1/0/+1
    float driftTimer        = 0.f;

    // Fade-in on capture to avoid clicks
    float fadeInEnv  = 0.f;
    static constexpr float kFadeInSamples = 256.f;

    // Decay envelope
    float decayEnvelope = 1.f;
    float decayCoeff    = 0.f;

    double sampleRate   = 44100.0;

    juce::Random rng;
};
