#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class ModMatrix
{
public:
    enum Source { LFO, ENVELOPE, kNumSources };
    enum Dest   { THRESHOLD, DEPTH, HAUNT, CRYSTALLIZE, POSSESSION, TONE, kNumDests };

    void prepare (const juce::dsp::ProcessSpec& spec);

    // RT-safe: amounts is a plain 2D array, no allocation
    void setRouting  (Source src, Dest dest, float amount);  // amount: -1.0–1.0
    void clearRoutings();

    float getModValue (Dest dest) const;
    float getLFOValue() const;

    // Call once per processBlock
    void process (float envelopeValue);

    void setLFORate (float rateHz);

private:
    // amounts[source][dest] — written from audio thread, always small fixed array
    float amounts[kNumSources][kNumDests] = {};
    float modValues[kNumDests]            = {};

    juce::dsp::Oscillator<float> lfo;
    float lfoValue    = 0.f;
    double sampleRate = 44100.0;
};
