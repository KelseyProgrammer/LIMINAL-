#include "LiminalEngine.h"

#include <cmath>

void LiminalEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    blockSize  = static_cast<int> (spec.maximumBlockSize);

    setSlew (200.f);

    hauntVerb.prepare  (spec);
    shimmer.prepare    (spec);
    pitchGhost.prepare (spec);

    dryBuffer.setSize (static_cast<int> (spec.numChannels),
                       static_cast<int> (spec.maximumBlockSize));

    currentBlend = 0.f;
    targetBlend  = 0.f;
}

void LiminalEngine::process (juce::AudioBuffer<float>& buffer, float envelopeLevel)
{
    // Detect downward threshold crossing to trigger PitchGhost capture
    if (lastEnvelopeLevel >= threshold && envelopeLevel < threshold)
        pitchGhost.triggerCapture (buffer);

    lastEnvelopeLevel = envelopeLevel;

    // Save dry signal
    dryBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());

    // Compute slewed blend
    targetBlend  = computeBlend (envelopeLevel);
    currentBlend += (targetBlend - currentBlend) * slewRate;

    // Clamp to avoid denormals drifting
    if (std::abs (currentBlend - targetBlend) < 1e-6f)
        currentBlend = targetBlend;

    // Update sub-engines with envelope level
    hauntVerb.setEnvelopeLevel (envelopeLevel);

    // Process wet engines
    hauntVerb.process  (buffer, currentBlend);
    shimmer.process    (buffer, currentBlend);
    pitchGhost.process (buffer, currentBlend);

    // Apply wet/dry mix
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        auto* dry = dryBuffer.getReadPointer (ch);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
            wet[s] = dry[s] * (1.f - mix) + wet[s] * mix;
    }
}

float LiminalEngine::computeBlend (float envelopeLevel)
{
    if (threshold <= 0.f) return depth;

    if (envelopeLevel < threshold)
        return (1.f - (envelopeLevel / threshold)) * depth;

    return 0.f;
}

void LiminalEngine::setThreshold (float t)
{
    threshold = juce::jlimit (0.f, 1.f, t);
}

void LiminalEngine::setSlew (float slewMs)
{
    if (slewMs <= 0.f || sampleRate <= 0.0)
    {
        slewRate = 1.f;
        return;
    }
    // One-pole coefficient from time constant
    const double tc = static_cast<double> (slewMs) / 1000.0;
    slewRate = static_cast<float> (1.0 - std::exp (-1.0 / (sampleRate * tc)));
}

void LiminalEngine::setDepth (float d)
{
    depth = juce::jlimit (0.f, 1.f, d);
}

void LiminalEngine::setMix (float m)
{
    mix = juce::jlimit (0.f, 1.f, m);
}

void LiminalEngine::setHaunt (float amount)
{
    hauntVerb.setHaunt (amount);
}

void LiminalEngine::setCrystallize (float amount)
{
    shimmer.setCrystallize (amount);
}

void LiminalEngine::setInterval (int semitones)
{
    shimmer.setInterval (semitones);
}

void LiminalEngine::setPossession (float amount)
{
    pitchGhost.setPossession (amount);
}

void LiminalEngine::onThresholdCrossedDown (const juce::AudioBuffer<float>& inputBuffer)
{
    pitchGhost.triggerCapture (inputBuffer);
}
