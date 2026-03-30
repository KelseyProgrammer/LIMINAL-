#include "ModMatrix.h"

#include <cmath>

void ModMatrix::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    lfo.prepare (spec);
    lfo.initialise ([] (float x) { return std::sin (x); });
    lfo.setFrequency (0.5f);

    clearRoutings();
}

void ModMatrix::setRouting (Source src, Dest dest, float amount)
{
    amounts[src][dest] = amount;
}

void ModMatrix::clearRoutings()
{
    for (int s = 0; s < kNumSources; ++s)
        for (int d = 0; d < kNumDests; ++d)
            amounts[s][d] = 0.f;

    for (int d = 0; d < kNumDests; ++d)
        modValues[d] = 0.f;
}

float ModMatrix::getModValue (Dest dest) const
{
    return modValues[dest];
}

float ModMatrix::getLFOValue() const
{
    return lfoValue;
}

void ModMatrix::process (float envelopeValue)
{
    // Tick LFO once per block (slow modulation — block-rate is fine)
    lfoValue = lfo.processSample (0.f);

    // Compute summed mod values for each destination
    const float sources[kNumSources] = { lfoValue, envelopeValue };

    for (int d = 0; d < kNumDests; ++d)
    {
        float sum = 0.f;
        for (int s = 0; s < kNumSources; ++s)
            sum += sources[s] * amounts[s][d];
        modValues[d] = sum;
    }
}

void ModMatrix::setLFORate (float rateHz)
{
    lfo.setFrequency (std::max (0.01f, rateHz));
}
