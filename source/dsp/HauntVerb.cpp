#include "HauntVerb.h"

#include <cmath>

static constexpr float kMinPreDelayMs = 10.f;
static constexpr float kMaxPreDelayMs = 60.f;
static constexpr float kHPFreqSilent  = 4000.f;  // Hz at full silence
static constexpr float kHPFreqLoud    = 80.f;     // Hz at full signal

// Allpass delay times per stage (in ms), slightly asymmetric per channel for stereo width
static constexpr float kDiffusionDelayMs[2][4] = {
    { 13.f, 17.f, 23.f, 31.f },   // left
    { 11.f, 19.f, 27.f, 37.f }    // right
};

void HauntVerb::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    const int maxPreDelaySamples = static_cast<int> (
        std::ceil (kMaxPreDelayMs / 1000.0 * sampleRate)) + 2;

    for (int ch = 0; ch < kNumChannels; ++ch)
    {
        preDelay[ch].prepare (spec);
        preDelay[ch].setMaximumDelayInSamples (maxPreDelaySamples);
        preDelay[ch].setDelay (static_cast<float> (kMinPreDelayMs / 1000.0 * sampleRate));

        for (int stage = 0; stage < kNumDiffusionStages; ++stage)
        {
            const int maxDiffSamples = static_cast<int> (
                std::ceil (kDiffusionDelayMs[ch][stage] / 1000.0 * sampleRate * 2.0)) + 2;

            diffusionLines[ch][stage].prepare (spec);
            diffusionLines[ch][stage].setMaximumDelayInSamples (maxDiffSamples);
            diffusionLines[ch][stage].setDelay (
                static_cast<float> (kDiffusionDelayMs[ch][stage] / 1000.0 * sampleRate));
        }
    }

    hpFilter.prepare (spec);
    hpFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpFilter.setCutoffFrequency (kHPFreqLoud);
    hpFilter.setResonance (0.7f);
}

void HauntVerb::process (juce::AudioBuffer<float>& buffer, float blendFactor)
{
    if (blendFactor <= 0.f)
        return;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = std::min (buffer.getNumChannels(), kNumChannels);

    // Update pre-delay length based on envelope
    const float preDelaySamples = static_cast<float> (
        computePreDelayMs (envelopeLevel) / 1000.0 * sampleRate);

    // Update HP cutoff
    hpFilter.setCutoffFrequency (computeHPFrequency (envelopeLevel));

    const float wetGain = blendFactor * hauntAmount;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        preDelay[ch].setDelay (preDelaySamples);

        auto* channelData = buffer.getWritePointer (ch);

        for (int s = 0; s < numSamples; ++s)
        {
            const float input = channelData[s];

            // Pre-delay
            preDelay[ch].pushSample (ch, input);
            float sig = preDelay[ch].popSample (ch);

            // 4-stage allpass diffusion
            for (int stage = 0; stage < kNumDiffusionStages; ++stage)
            {
                const float g   = diffusionCoeffs[stage];
                const float del = diffusionLines[ch][stage].popSample (ch);
                const float v   = sig - g * del;
                diffusionLines[ch][stage].pushSample (ch, v);
                sig = del + g * v;
            }

            // HP sweep (applied per-channel via mono filter; acceptable for a reverb tail)
            sig = hpFilter.processSample (ch, sig);

            // Blend wet onto dry
            channelData[s] += sig * wetGain;
        }
    }
}

void HauntVerb::setHaunt (float amount)
{
    hauntAmount = juce::jlimit (0.f, 1.f, amount);
}

void HauntVerb::setEnvelopeLevel (float level)
{
    envelopeLevel = juce::jlimit (0.f, 1.f, level);
}

float HauntVerb::computeHPFrequency (float level) const
{
    // level 0 (silence) → kHPFreqSilent (4kHz)
    // level 1 (loud)    → kHPFreqLoud   (80Hz)
    // Log interpolation for more perceptual linearity
    const float logLow  = std::log (kHPFreqLoud);
    const float logHigh = std::log (kHPFreqSilent);
    return std::exp (logLow + (1.f - level) * (logHigh - logLow));
}

float HauntVerb::computePreDelayMs (float level) const
{
    // level 0 (silence) → kMaxPreDelayMs (60ms)
    // level 1 (loud)    → kMinPreDelayMs (10ms)
    return kMinPreDelayMs + (1.f - level) * (kMaxPreDelayMs - kMinPreDelayMs);
}
