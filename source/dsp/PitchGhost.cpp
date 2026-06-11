#include "PitchGhost.h"

#include <cmath>

void PitchGhost::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    ringBuffer.setSize (static_cast<int> (spec.numChannels), kRingSize);
    ringBuffer.clear();
    ringWritePos = 0;

    for (auto& g : ghosts)
    {
        g.capture.setSize (static_cast<int> (spec.numChannels), kMaxCaptureSamples);
        g.capture.clear();
        g.length       = 0;
        g.readPos      = 0.f;
        g.active       = false;
        g.currentCents = 0.f;
        g.targetCents  = 0.f;
        g.driftTimer   = 0.f;
        g.fadeInEnv    = 0.f;
        g.decayEnv     = 0.f;
    }
    nextGhost = 0;

    setGhostDecay (1400.f);   // generous default so the choir overlaps
}

void PitchGhost::renderWet (juce::AudioBuffer<float>& wet, float blendFactor)
{
    if (blendFactor <= 0.f)
    {
        // Ghosts die quietly when the engines sleep mid-phrase
        return;
    }

    const int numSamples = wet.getNumSamples();
    const float dt = 1.f / static_cast<float> (sampleRate);

    for (auto& g : ghosts)
    {
        if (! g.active)
            continue;

        const int numChannels = juce::jmin (wet.getNumChannels(),
                                            g.capture.getNumChannels());

        for (int s = 0; s < numSamples; ++s)
        {
            if (g.fadeInEnv < 1.f)
                g.fadeInEnv = juce::jmin (1.f, g.fadeInEnv + 1.f / kFadeInSamples);

            g.decayEnv *= decayCoeff;
            if (g.decayEnv < 1e-5f)
            {
                g.active = false;
                break;
            }

            updateDrift (g, dt);
            const float ratio = pitchToRatio (g.currentCents);

            const float env = g.decayEnv * g.fadeInEnv * blendFactor * 0.8f;

            // Crossfade window near loop boundaries to prevent wrap clicks
            const float fadeZone  = juce::jmin (256.f, static_cast<float> (g.length) * 0.05f);
            const float distToEnd = static_cast<float> (g.length) - g.readPos;
            const float loopGain  = juce::jmin (1.f,
                                     juce::jmin (g.readPos, distToEnd) / fadeZone);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const int   i0   = static_cast<int> (g.readPos) % g.length;
                const int   i1   = (i0 + 1) % g.length;
                const float frac = g.readPos - std::floor (g.readPos);

                const auto* cap = g.capture.getReadPointer (ch);
                const float ghostSample = cap[i0] * (1.f - frac) + cap[i1] * frac;

                wet.getWritePointer (ch)[s] += ghostSample * env * loopGain;
            }

            g.readPos = std::fmod (g.readPos + ratio, static_cast<float> (g.length));
        }
    }
}

void PitchGhost::pushAudio (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(),
                                        ringBuffer.getNumChannels());

    for (int s = 0; s < numSamples; ++s)
    {
        const int pos = ringWritePos & (kRingSize - 1);
        for (int ch = 0; ch < numChannels; ++ch)
            ringBuffer.getWritePointer (ch)[pos] = buffer.getReadPointer (ch)[s];
        ringWritePos = (ringWritePos + 1) & (kRingSize - 1);
    }
}

void PitchGhost::triggerCapture()
{
    Ghost& g = ghosts[nextGhost];
    nextGhost = (nextGhost + 1) % kNumGhosts;

    g.length = juce::jmax (1, juce::jmin (kMaxCaptureSamples,
                                          static_cast<int> (sampleRate * 0.25)));

    const int numCh = juce::jmin (ringBuffer.getNumChannels(),
                                  g.capture.getNumChannels());

    // Copy backwards from the ring write head (the most recent audio)
    const int startPos = (ringWritePos - g.length + kRingSize) & (kRingSize - 1);

    for (int ch = 0; ch < numCh; ++ch)
    {
        const auto* src = ringBuffer.getReadPointer (ch);
        auto*       dst = g.capture.getWritePointer (ch);
        for (int i = 0; i < g.length; ++i)
            dst[i] = src[(startPos + i) & (kRingSize - 1)];
    }

    g.readPos      = 0.f;
    g.decayEnv     = 1.f;
    g.fadeInEnv    = 0.f;
    g.currentCents = 0.f;
    g.targetCents  = 0.f;
    g.driftTimer   = 0.f;
    g.active       = true;
}

void PitchGhost::setPossession (float amount)
{
    possession = juce::jlimit (0.f, 1.f, amount);
}

void PitchGhost::setDriftRate (float rateHz)
{
    driftRate = juce::jmax (0.01f, rateHz);
}

void PitchGhost::setDriftDirection (int direction)
{
    driftDir = juce::jlimit (-1, 1, direction);
}

void PitchGhost::setGhostDecay (float decayMs)
{
    if (decayMs <= 0.f || sampleRate <= 0.0)
    {
        decayCoeff = 0.f;
        return;
    }
    decayCoeff = static_cast<float> (
        std::exp (-1.0 / (sampleRate * static_cast<double> (decayMs) / 1000.0)));
}

void PitchGhost::updateDrift (Ghost& g, float deltaTime)
{
    g.driftTimer += deltaTime;

    if (g.driftTimer >= (1.f / driftRate))
    {
        g.driftTimer = 0.f;

        // Drift range: possession 1 → ±12 cents (tight double),
        // possession 0 → ±1200 cents (a full octave of wandering)
        const float t        = 1.f - possession;
        const float maxDrift = 12.f + t * t * 1188.f;

        if (driftDir == 0)
            g.targetCents = (rng.nextFloat() * 2.f - 1.f) * maxDrift;
        else
            g.targetCents = static_cast<float> (driftDir) * maxDrift
                            * (0.4f + 0.6f * rng.nextFloat());
    }

    // Smooth glide toward the target; slower for wide drifts so big pitch
    // excursions sound like a slide, not a jump
    const float driftSmooth = 0.002f;
    g.currentCents += (g.targetCents - g.currentCents) * driftSmooth;
}

float PitchGhost::pitchToRatio (float cents) const
{
    return std::pow (2.f, cents / 1200.f);
}
