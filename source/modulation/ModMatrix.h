#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <vector>

class ModMatrix
{
public:
    enum Source { LFO, ENVELOPE };
    enum Dest   { THRESHOLD, DEPTH, HAUNT, CRYSTALLIZE, POSSESSION, TONE, kNumDests };

    void prepare (const juce::dsp::ProcessSpec& spec);

    void setRouting (Source src, Dest dest, float amount);  // amount: -1.0–1.0
    void clearRoutings();

    float getModValue (Dest dest) const;
    float getLFOValue() const;

    // Call once per processBlock
    void process (float lfoExternalValue, float envelopeValue);

    void setLFORate (float rateHz);

private:
    struct Route { Source src; Dest dest; float amount; };
    std::vector<Route> routes;

    float modValues[kNumDests] = {};

    juce::dsp::Oscillator<float> lfo;
    float lfoValue    = 0.f;
    double sampleRate = 44100.0;
};
