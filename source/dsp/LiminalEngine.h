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

    void setThreshold  (float normalizedThreshold);  // 0.0–1.0
    void setSlew       (float slewMs);
    void setDepth      (float depth);               // 0.0–1.0
    void setMix        (float mix);                 // 0.0–1.0
    void setTone       (float t);                   // -1.0 (dark) to +1.0 (bright)
    void setInvertMode (bool invert);               // flip threshold polarity

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

    void  applyTone (juce::AudioBuffer<float>& buffer);

    float threshold    = 0.05f;
    float depth        = 1.0f;
    float mix          = 1.0f;
    float tone         = 0.f;
    float lastEnvelopeLevel = 1.0f;
    bool  invertMode   = false;

    float toneState[2] = {};   // one-pole IIR state per channel

    double sampleRate  = 44100.0;
    int    blockSize   = 512;

    HauntVerb  hauntVerb;
    Shimmer    shimmer;
    PitchGhost pitchGhost;

    juce::AudioBuffer<float> dryBuffer;
};
