#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "HauntVerb.h"
#include "Shimmer.h"
#include "PitchGhost.h"

// Master DSP coordinator.
//
// Envelope levels are normalized dB units: 0.0 = -60dB (silence), 1.0 = 0dBFS.
// Engines run in PARALLEL off the dry signal and their wet buses are summed:
//
//   shimmerWet = Shimmer(dry)
//   verbWet    = HauntVerb(dry + shimmerWet * send)   ← shimmer washes into the verb
//   ghostWet   = PitchGhost()
//   out        = dry + (shimmerWet + verbWet + ghostWet) * mix
//
// The dry signal always passes (this is a negative-space effect — the engines
// are silent while you play), so MIX scales the conjured atmosphere only.
class LiminalEngine
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer, float envelopeLevel);

    void setThreshold  (float normalizedThreshold);  // 0.0–1.0 (≈ -60..0 dB)
    void setSlew       (float slewMs);
    void setDepth      (float depth);               // 0.0–1.0
    void setMix        (float mix);                 // 0.0–1.0
    void setTone       (float t);                   // -1.0 (dark) to +1.0 (bright)
    void setInvertMode (bool invert);               // flip threshold polarity

    // Engine parameter passthrough
    void setHaunt          (float amount);
    void setCrystallize    (float amount);
    void setInterval       (int semitones);
    void setPossession     (float amount);
    void setDriftRate      (float rateHz);
    void setDriftDirection (int direction);

    float getCurrentBlend() const { return currentBlend; }
    bool  isAboveThreshold() const { return lastEnvelopeLevel >= threshold; }

    // External capture trigger (MIDI note-on)
    void triggerGhostCapture() { pitchGhost.triggerCapture(); }

private:
    float computeBlend (float envelopeLevel);
    void  applyTone (juce::AudioBuffer<float>& buffer);

    float currentBlend = 0.f;
    float targetBlend  = 0.f;
    float slewRate     = 0.f;

    float threshold    = 0.5f;
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
    juce::AudioBuffer<float> shimmerBus;
    juce::AudioBuffer<float> verbInBus;
    juce::AudioBuffer<float> verbBus;
    juce::AudioBuffer<float> ghostBus;
};
