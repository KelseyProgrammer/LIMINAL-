#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Phantom playback of micro-snapshots captured at threshold crossings.
//
// Up to kNumGhosts voices play simultaneously (round-robin on capture), so
// staccato playing accumulates a small choir of phantoms, each drifting away
// from the source pitch independently.
//
// Drift range scales with (1 - possession): at possession 0 a ghost can
// wander up to an octave from the source — genuinely alien — while at
// possession 1 it stays within a few cents, a tight shimmer double.
class PitchGhost
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);

    // Renders the summed ghost voices into `wet` (wet-only, additive over clear).
    void renderWet (juce::AudioBuffer<float>& wet, float blendFactor);

    void setPossession    (float amount);       // 0.0–1.0, drift intensity
    void setDriftRate     (float rateHz);       // how often a ghost picks a new target
    void setDriftDirection(int direction);      // -1=down, 0=wander, 1=up
    void setGhostDecay    (float decayMs);

    // Feed audio continuously so the ring buffer stays current
    void pushAudio (const juce::AudioBuffer<float>& buffer);

    // Call when the envelope crosses the threshold — spawns the next ghost voice
    void triggerCapture();

private:
    float pitchToRatio (float cents) const;

    static constexpr int kNumGhosts         = 3;
    static constexpr int kRingSize          = 65536;  // power-of-2 for fast modulo
    static constexpr int kMaxCaptureSamples = 24000;  // up to ~500ms at 48kHz
    static constexpr float kFadeInSamples   = 256.f;

    juce::AudioBuffer<float> ringBuffer;
    int ringWritePos = 0;

    struct Ghost
    {
        juce::AudioBuffer<float> capture;
        int   length        = 0;
        float readPos       = 0.f;
        bool  active        = false;

        float currentCents  = 0.f;
        float targetCents   = 0.f;
        float driftTimer    = 0.f;

        float fadeInEnv     = 0.f;
        float decayEnv      = 0.f;
    };

    Ghost ghosts[kNumGhosts];
    int   nextGhost = 0;

    void updateDrift (Ghost& g, float deltaTime);

    float possession = 0.5f;
    float driftRate  = 0.5f;   // Hz
    int   driftDir   = 0;      // -1/0/+1

    float decayCoeff = 0.f;

    double sampleRate = 44100.0;

    juce::Random rng;
};
