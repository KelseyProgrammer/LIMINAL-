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
    // Keep ring buffer current so PitchGhost always has recent audio to capture
    pitchGhost.pushAudio (buffer);

    // Detect threshold crossing to trigger PitchGhost capture
    // Normal: falling below threshold. Invert: rising above threshold.
    const bool crossedNormal  = !invertMode && lastEnvelopeLevel >= threshold && envelopeLevel < threshold;
    const bool crossedInvert  =  invertMode && lastEnvelopeLevel <= threshold && envelopeLevel > threshold;
    if (crossedNormal || crossedInvert)
        pitchGhost.triggerCapture();

    lastEnvelopeLevel = envelopeLevel;

    // Save dry signal
    dryBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());

    // Compute slewed blend — apply N samples of one-pole IIR correctly
    // slewRate is a per-sample coefficient; raise (1-slewRate)^N to get the
    // block-accurate decay so convergence is block-size independent.
    targetBlend = computeBlend (envelopeLevel);
    {
        const float decay = std::pow (1.f - slewRate,
                                      static_cast<float> (buffer.getNumSamples()));
        currentBlend = targetBlend + (currentBlend - targetBlend) * decay;
    }

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

    // Apply tone shaping to final output
    applyTone (buffer);
}

float LiminalEngine::computeBlend (float envelopeLevel)
{
    if (!invertMode)
    {
        if (threshold <= 0.f) return depth;
        if (envelopeLevel < threshold)
            return (1.f - (envelopeLevel / threshold)) * depth;
        return 0.f;
    }
    else
    {
        // Invert: engines wake above threshold (react to loud signals)
        const float headroom = 1.f - threshold;
        if (headroom <= 0.f) return depth;
        if (envelopeLevel > threshold)
            return ((envelopeLevel - threshold) / headroom) * depth;
        return 0.f;
    }
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

void LiminalEngine::onThresholdCrossedDown (const juce::AudioBuffer<float>& /*inputBuffer*/)
{
    pitchGhost.triggerCapture();
}

void LiminalEngine::setTone (float t)
{
    tone = juce::jlimit (-1.f, 1.f, t);
}

void LiminalEngine::setInvertMode (bool invert)
{
    invertMode = invert;
}

void LiminalEngine::applyTone (juce::AudioBuffer<float>& buffer)
{
    if (std::abs (tone) < 0.001f) return;

    // One-pole IIR crossover at ~2kHz
    // tone < 0 → blend toward LP (darker)
    // tone > 0 → blend toward HP (brighter)
    const float fc = 2000.f;
    const float a  = std::exp (-juce::MathConstants<float>::twoPi * fc
                                / static_cast<float> (sampleRate));
    const float amount = std::abs (tone);

    for (int ch = 0; ch < std::min (buffer.getNumChannels(), 2); ++ch)
    {
        auto*  data = buffer.getWritePointer (ch);
        float  z    = toneState[ch];

        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            const float x  = data[s];
            z = (1.f - a) * x + a * z;          // LP
            const float hp = x - z;              // HP = input - LP

            data[s] = x + amount * ((tone < 0.f ? z : hp) - x);
        }

        toneState[ch] = z;
    }
}
