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

    // Call when envelope crosses threshold downward
    void triggerCapture (const juce::AudioBuffer<float>& inputBuffer);

private:
    void  updateDrift  (float deltaTime);
    float pitchToRatio (float cents) const;

    // Capture buffer (holds the snapshot just before silence)
    static constexpr int kMaxCaptureSamples = 256;
    juce::AudioBuffer<float> captureBuffer;
    int   captureLength  = 256;
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
