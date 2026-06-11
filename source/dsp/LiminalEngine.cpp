#include "LiminalEngine.h"

#include <cmath>

// Send level from the shimmer bus into the verb input — this is what turns
// the pitch cascade into a wash instead of a discrete harmonizer.
static constexpr float kShimmerToVerbSend = 0.85f;

// Blend falloff in normalized dB units (0.4 = 24 dB below threshold reaches
// full blend). Clamped to the threshold itself so low thresholds can still
// reach full blend at true silence.
static float blendFalloff (float span)
{
    return juce::jlimit (0.05f, 0.4f, span);
}

void LiminalEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    blockSize  = static_cast<int> (spec.maximumBlockSize);

    setSlew (200.f);

    hauntVerb.prepare  (spec);
    shimmer.prepare    (spec);
    pitchGhost.prepare (spec);

    const int ch = static_cast<int> (spec.numChannels);
    const int n  = static_cast<int> (spec.maximumBlockSize);
    dryBuffer .setSize (ch, n);
    shimmerBus.setSize (ch, n);
    verbInBus .setSize (ch, n);
    verbBus   .setSize (ch, n);
    ghostBus  .setSize (ch, n);

    currentBlend = 0.f;
    targetBlend  = 0.f;
}

void LiminalEngine::process (juce::AudioBuffer<float>& buffer, float envelopeLevel)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Keep ring buffer current so PitchGhost always has recent audio to capture
    pitchGhost.pushAudio (buffer);

    // Detect threshold crossing to trigger PitchGhost capture.
    // Normal: falling below threshold. Invert: rising above threshold.
    const bool crossedNormal = !invertMode && lastEnvelopeLevel >= threshold && envelopeLevel < threshold;
    const bool crossedInvert =  invertMode && lastEnvelopeLevel <= threshold && envelopeLevel > threshold;
    if (crossedNormal || crossedInvert)
        pitchGhost.triggerCapture();

    lastEnvelopeLevel = envelopeLevel;

    // Save dry signal
    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // Compute slewed blend — apply N samples of one-pole IIR correctly
    targetBlend = computeBlend (envelopeLevel);
    {
        const float decay = std::pow (1.f - slewRate, static_cast<float> (numSamples));
        currentBlend = targetBlend + (currentBlend - targetBlend) * decay;
    }
    if (std::abs (currentBlend - targetBlend) < 1e-6f)
        currentBlend = targetBlend;

    hauntVerb.setEnvelopeLevel (envelopeLevel);

    // ── Parallel engine buses ─────────────────────────────────────────────────
    shimmerBus.setSize (numChannels, numSamples, false, false, true);
    verbInBus .setSize (numChannels, numSamples, false, false, true);
    verbBus   .setSize (numChannels, numSamples, false, false, true);
    ghostBus  .setSize (numChannels, numSamples, false, false, true);

    shimmer.renderWet (dryBuffer, shimmerBus, currentBlend);

    // Verb input = dry + shimmer send: the cascade washes into the tank
    for (int ch = 0; ch < numChannels; ++ch)
    {
        verbInBus.copyFrom (ch, 0, dryBuffer, ch, 0, numSamples);
        verbInBus.addFrom  (ch, 0, shimmerBus, ch, 0, numSamples, kShimmerToVerbSend);
    }
    hauntVerb.renderWet (verbInBus, verbBus, currentBlend);

    ghostBus.clear();
    pitchGhost.renderWet (ghostBus, currentBlend);

    // ── Sum: dry always passes; MIX scales the conjured atmosphere ───────────
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* out = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer .getReadPointer (ch);
        const auto* sh  = shimmerBus.getReadPointer (ch);
        const auto* vb  = verbBus   .getReadPointer (ch);
        const auto* gh  = ghostBus  .getReadPointer (ch);

        for (int s = 0; s < numSamples; ++s)
            out[s] = dry[s] + (sh[s] + vb[s] + gh[s]) * mix;
    }

    // Apply tone shaping to final output
    applyTone (buffer);
}

float LiminalEngine::computeBlend (float envelopeLevel)
{
    if (! invertMode)
    {
        if (threshold <= 0.f) return 0.f;
        if (envelopeLevel < threshold)
            return juce::jmin (1.f, (threshold - envelopeLevel)
                                    / blendFalloff (threshold)) * depth;
        return 0.f;
    }

    // Invert: engines wake above threshold (react to loud signals)
    const float headroom = 1.f - threshold;
    if (headroom <= 0.f) return 0.f;
    if (envelopeLevel > threshold)
        return juce::jmin (1.f, (envelopeLevel - threshold)
                                / blendFalloff (headroom)) * depth;
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
    const double tc = static_cast<double> (slewMs) / 1000.0;
    slewRate = static_cast<float> (1.0 - std::exp (-1.0 / (sampleRate * tc)));
}

void LiminalEngine::setDepth (float d)              { depth = juce::jlimit (0.f, 1.f, d); }
void LiminalEngine::setMix (float m)                { mix = juce::jlimit (0.f, 1.f, m); }
void LiminalEngine::setHaunt (float amount)         { hauntVerb.setHaunt (amount); }
void LiminalEngine::setCrystallize (float amount)   { shimmer.setCrystallize (amount); }
void LiminalEngine::setInterval (int semitones)     { shimmer.setInterval (semitones); }
void LiminalEngine::setPossession (float amount)    { pitchGhost.setPossession (amount); }
void LiminalEngine::setDriftRate (float rateHz)     { pitchGhost.setDriftRate (rateHz); }
void LiminalEngine::setDriftDirection (int dir)     { pitchGhost.setDriftDirection (dir); }
void LiminalEngine::setTone (float t)               { tone = juce::jlimit (-1.f, 1.f, t); }
void LiminalEngine::setInvertMode (bool invert)     { invertMode = invert; }

void LiminalEngine::applyTone (juce::AudioBuffer<float>& buffer)
{
    if (std::abs (tone) < 0.001f) return;

    // One-pole IIR crossover at ~2kHz
    // tone < 0 → blend toward LP (darker), tone > 0 → toward HP (brighter)
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
