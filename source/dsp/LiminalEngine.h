#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "HauntVerb.h"
#include "Shimmer.h"
#include "PitchGhost.h"

class LiminalEngine
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer, float envelopeLevel);

    void setThreshold (float normalizedThreshold);  // 0.0–1.0
    void setSlew      (float slewMs);
    void setDepth     (float depth);                // 0.0–1.0
    void setMix       (float mix);                  // 0.0–1.0

    // Engine parameter passthrough
    void setHaunt       (float amount);
    void setCrystallize (float amount);
    void setInterval    (int semitones);
    void setPossession  (float amount);

    float getCurrentBlend() const { return currentBlend; }
    bool  isAboveThreshold() const { return lastEnvelopeLevel >= threshold; }

    // Called externally when envelope crosses threshold downward
    void onThresholdCrossedDown (const juce::AudioBuffer<float>& inputBuffer);

private:
    float computeBlend (float envelopeLevel);

    float currentBlend = 0.f;
    float targetBlend  = 0.f;
    float slewRate     = 0.f;

    float threshold    = 0.3f;
    float depth        = 1.0f;
    float mix          = 1.0f;
    float lastEnvelopeLevel = 1.0f;

    double sampleRate  = 44100.0;
    int    blockSize   = 512;

    HauntVerb  hauntVerb;
    Shimmer    shimmer;
    PitchGhost pitchGhost;

    juce::AudioBuffer<float> dryBuffer;
};
