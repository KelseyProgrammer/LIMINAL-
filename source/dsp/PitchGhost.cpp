#include "PitchGhost.h"

#include <cmath>

void PitchGhost::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    ringBuffer.setSize (static_cast<int> (spec.numChannels), kRingSize);
    ringBuffer.clear();
    ringWritePos = 0;

    captureBuffer.setSize (static_cast<int> (spec.numChannels), kMaxCaptureSamples);
    captureBuffer.clear();

    setGhostDecay (800.f);   // 800 ms default decay

    ghostActive         = false;
    currentPitchCents   = 0.f;
    targetPitchCents    = 0.f;
    driftTimer          = 0.f;
    decayEnvelope       = 0.f;
    fadeInEnv           = 0.f;
    captureReadPos      = 0.f;
}

void PitchGhost::process (juce::AudioBuffer<float>& buffer, float blendFactor)
{
    if (!ghostActive || blendFactor <= 0.f)
        return;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = std::min (buffer.getNumChannels(),
                                      captureBuffer.getNumChannels());

    const float dt = 1.f / static_cast<float> (sampleRate);

    for (int s = 0; s < numSamples; ++s)
    {
        // Fade-in envelope (avoids click on capture start)
        if (fadeInEnv < 1.f)
            fadeInEnv = std::min (1.f, fadeInEnv + 1.f / kFadeInSamples);

        // Decay envelope
        decayEnvelope *= decayCoeff;

        // Update pitch drift
        updateDrift (dt);
        const float ratio = pitchToRatio (currentPitchCents);

        if (decayEnvelope < 1e-5f)
        {
            ghostActive = false;
            break;
        }

        const float env = decayEnvelope * fadeInEnv * blendFactor;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            // Linear interpolation from capture buffer (looping)
            const float fIdx  = captureReadPos;
            const int   iIdx  = static_cast<int> (fIdx) % captureLength;
            const int   iIdx1 = (iIdx + 1) % captureLength;
            const float frac  = fIdx - static_cast<float> (static_cast<int> (fIdx));

            const auto* cap = captureBuffer.getReadPointer (ch);
            const float ghostSample = cap[iIdx] * (1.f - frac) + cap[iIdx1] * frac;

            buffer.getWritePointer (ch)[s] += ghostSample * env;
        }

        captureReadPos = std::fmod (captureReadPos + ratio, static_cast<float> (captureLength));
    }
}

void PitchGhost::pushAudio (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = std::min (buffer.getNumChannels(),
                                      ringBuffer.getNumChannels());

    for (int s = 0; s < numSamples; ++s)
    {
        const int pos = ringWritePos & (kRingSize - 1);   // fast power-of-2 modulo
        for (int ch = 0; ch < numChannels; ++ch)
            ringBuffer.getWritePointer (ch)[pos] = buffer.getReadPointer (ch)[s];
        ringWritePos = (ringWritePos + 1) & (kRingSize - 1);
    }
}

void PitchGhost::triggerCapture()
{
    // Capture the last 250ms worth of audio from the ring buffer
    captureLength = std::min (kMaxCaptureSamples,
                              static_cast<int> (sampleRate * 0.25));
    captureLength = std::max (captureLength, 1);   // safety

    const int numCh  = std::min (ringBuffer.getNumChannels(),
                                 captureBuffer.getNumChannels());

    // Copy backwards from ring write head (the most recent audio)
    const int startPos = (ringWritePos - captureLength + kRingSize) & (kRingSize - 1);

    for (int ch = 0; ch < numCh; ++ch)
    {
        const auto* src = ringBuffer.getReadPointer (ch);
        auto*       dst = captureBuffer.getWritePointer (ch);

        for (int i = 0; i < captureLength; ++i)
            dst[i] = src[(startPos + i) & (kRingSize - 1)];
    }

    captureReadPos    = 0.f;
    decayEnvelope     = 1.f;
    fadeInEnv         = 0.f;
    currentPitchCents = 0.f;
    targetPitchCents  = 0.f;
    driftTimer        = 0.f;
    ghostActive       = true;
}

void PitchGhost::setPossession (float amount)
{
    possession = juce::jlimit (0.f, 1.f, amount);
}

void PitchGhost::setDriftRate (float rateHz)
{
    driftRate = std::max (0.001f, rateHz);
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

void PitchGhost::updateDrift (float deltaTime)
{
    driftTimer += deltaTime;

    if (driftTimer >= (1.f / driftRate))
    {
        driftTimer = 0.f;

        // Max drift range: low possession → wide drift, high possession → narrow
        const float maxDrift = (1.f - possession) * 50.f;  // up to ±50 cents

        if (driftDir == 0)
        {
            // Wander: pick new random target
            targetPitchCents = (rng.nextFloat() * 2.f - 1.f) * maxDrift;
        }
        else
        {
            // Directional drift
            targetPitchCents = static_cast<float> (driftDir) * maxDrift;
        }
    }

    // Smooth drift toward target (fast enough to be musical)
    const float driftSmooth = 0.01f;
    currentPitchCents += (targetPitchCents - currentPitchCents) * driftSmooth;
}

float PitchGhost::pitchToRatio (float cents) const
{
    return std::pow (2.f, cents / 1200.f);
}
