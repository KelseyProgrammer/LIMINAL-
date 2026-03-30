#include "ModMatrix.h"

#include <cmath>

void ModMatrix::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    lfo.prepare (spec);
    lfo.initialise ([] (float x) { return std::sin (x); });
    lfo.setFrequency (0.5f);  // 0.5 Hz default LFO rate

    std::fill (std::begin (modValues), std::end (modValues), 0.f);
}

void ModMatrix::setRouting (Source src, Dest dest, float amount)
{
    // Replace existing route for the same src+dest pair
    for (auto& r : routes)
    {
        if (r.src == src && r.dest == dest)
        {
            r.amount = amount;
            return;
        }
    }
    routes.push_back ({ src, dest, amount });
}

void ModMatrix::clearRoutings()
{
    routes.clear();
    std::fill (std::begin (modValues), std::end (modValues), 0.f);
}

float ModMatrix::getModValue (Dest dest) const
{
    return modValues[dest];
}

float ModMatrix::getLFOValue() const
{
    return lfoValue;
}

void ModMatrix::process (float /*lfoExternalValue*/, float envelopeValue)
{
    // Tick LFO by one sample (called once per block — approximate, but fine for slow modulation)
    lfoValue = lfo.processSample (0.f);

    std::fill (std::begin (modValues), std::end (modValues), 0.f);

    for (const auto& r : routes)
    {
        const float src = (r.src == LFO) ? lfoValue : envelopeValue;
        modValues[r.dest] += src * r.amount;
    }
}

void ModMatrix::setLFORate (float rateHz)
{
    lfo.setFrequency (std::max (0.01f, rateHz));
}
