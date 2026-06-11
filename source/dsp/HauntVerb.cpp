#include "HauntVerb.h"

#include <cmath>

static constexpr float kMinPreDelayMs = 10.f;
static constexpr float kMaxPreDelayMs = 60.f;
static constexpr float kHPFreqSilent  = 4000.f;  // Hz at full silence
static constexpr float kHPFreqLoud    = 80.f;    // Hz at full signal

// Allpass delay times per stage (in ms), slightly asymmetric per channel for stereo width
static constexpr float kDiffusionDelayMs[2][4] = {
    { 13.f, 17.f, 23.f, 31.f },   // left
    { 11.f, 19.f, 27.f, 37.f }    // right
};

// Tank delay times (ms) — mutually prime-ish, asymmetric L/R
static constexpr float kTankDelayMs[2] = { 97.f, 113.f };
static constexpr float kTankModRateHz  = 0.23f;   // slow chorus of the tank
static constexpr float kTankModDepthMs = 0.9f;
static constexpr float kTankDampCoeff  = 0.32f;   // one-pole LP in the loop (~3kHz)

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

            diffFbLP[ch][stage] = 0.f;
        }

        tankBaseDelaySamples[ch] = static_cast<float> (kTankDelayMs[ch] / 1000.0 * sampleRate);

        const int maxTankSamples = static_cast<int> (
            std::ceil ((kTankDelayMs[ch] + kTankModDepthMs * 2.f) / 1000.0 * sampleRate)) + 4;
        tankLine[ch].prepare (spec);
        tankLine[ch].setMaximumDelayInSamples (maxTankSamples);
        tankLine[ch].setDelay (tankBaseDelaySamples[ch]);

        tankDamp[ch] = 0.f;
    }

    tankModPhase = 0.f;
    tankModInc   = static_cast<float> (juce::MathConstants<double>::twoPi
                                       * kTankModRateHz / sampleRate);
    tankEnergy   = 0.f;

    hpFilter.prepare (spec);
    hpFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpFilter.setCutoffFrequency (kHPFreqLoud);
    hpFilter.setResonance (0.7f);
}

void HauntVerb::renderWet (const juce::AudioBuffer<float>& in,
                           juce::AudioBuffer<float>& wet,
                           float blendFactor)
{
    const int numSamples  = in.getNumSamples();
    const int numChannels = juce::jmin (in.getNumChannels(), wet.getNumChannels(),
                                        static_cast<int> (kNumChannels));

    wet.clear();

    // Skip all work only when there is nothing to do AND the tank has rung out —
    // an active tank must keep decaying after blend drops, or the tail truncates.
    if (blendFactor <= 0.f && ! isTankActive())
        return;

    // Update pre-delay length and HP cutoff from the envelope
    const float preDelaySamples = static_cast<float> (
        computePreDelayMs (envelopeLevel) / 1000.0 * sampleRate);
    hpFilter.setCutoffFrequency (computeHPFrequency (envelopeLevel));

    // HAUNT sets both wet level and tank decay (0.55 = short, 0.95 = vast)
    const float tankFb    = 0.55f + hauntAmount * 0.40f;
    const float modDepth  = static_cast<float> (kTankModDepthMs / 1000.0 * sampleRate);

    const float* inPtr[kNumChannels]  = {};
    float*       wetPtr[kNumChannels] = {};
    for (int ch = 0; ch < numChannels; ++ch)
    {
        inPtr[ch]  = in.getReadPointer (ch);
        wetPtr[ch] = wet.getWritePointer (ch);
        preDelay[ch].setDelay (preDelaySamples);
    }

    float energy = tankEnergy;

    for (int s = 0; s < numSamples; ++s)
    {
        // Slow sine modulation, quadrature between channels
        tankModPhase += tankModInc;
        if (tankModPhase > juce::MathConstants<float>::twoPi)
            tankModPhase -= juce::MathConstants<float>::twoPi;
        const float modL = std::sin (tankModPhase);
        const float modR = std::cos (tankModPhase);

        float diffOut[kNumChannels] = {};

        for (int ch = 0; ch < numChannels; ++ch)
        {
            // Engine input is gated by blend: silent input keeps the tank
            // decaying naturally instead of being re-fed.
            const float input = inPtr[ch][s] * blendFactor;

            // Pre-delay
            preDelay[ch].pushSample (ch, input);
            float sig = preDelay[ch].popSample (ch);

            // 4-stage allpass diffusion with LP in the feedback path
            for (int stage = 0; stage < kNumDiffusionStages; ++stage)
            {
                const float g   = diffusionCoeffs[stage];
                const float del = diffusionLines[ch][stage].popSample (ch);
                const float v   = sig - g * del;
                const float lp  = 0.28f;
                diffFbLP[ch][stage] = lp * v + (1.f - lp) * diffFbLP[ch][stage];
                diffusionLines[ch][stage].pushSample (ch, diffFbLP[ch][stage]);
                sig = del + g * diffFbLP[ch][stage];
            }

            diffOut[ch] = sig;
        }

        // ── Cross-coupled tank (figure-8) ────────────────────────────────────
        float tankOut[kNumChannels] = {};
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float mod = (ch == 0 ? modL : modR);
            tankOut[ch] = tankLine[ch].popSample (
                ch, tankBaseDelaySamples[ch] + mod * modDepth, true);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const int other = (numChannels > 1) ? (1 - ch) : ch;

            // Damping LP inside the loop keeps repeated reflections dark
            tankDamp[ch] = kTankDampCoeff * tankOut[other]
                         + (1.f - kTankDampCoeff) * tankDamp[ch];

            tankLine[ch].pushSample (ch, diffOut[ch] + tankDamp[ch] * tankFb);
        }

        // ── Output: early diffusion + late tank, through the HP sweep ────────
        // The input feed is already blend-gated, so the output is scaled by
        // hauntAmount only — the tail rings out naturally when blend falls.
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float out = diffOut[ch] * 0.30f + tankOut[ch] * 0.85f;
            out = hpFilter.processSample (ch, out);
            wetPtr[ch][s] = out * hauntAmount;
        }

        energy = 0.999f * energy
               + 0.001f * (std::abs (tankOut[0]) + std::abs (tankOut[numChannels - 1]));
    }

    tankEnergy = energy;
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
    // level 0 (silence) → kHPFreqSilent (4kHz), level 1 (loud) → kHPFreqLoud (80Hz)
    const float logLow  = std::log (kHPFreqLoud);
    const float logHigh = std::log (kHPFreqSilent);
    return std::exp (logLow + (1.f - level) * (logHigh - logLow));
}

float HauntVerb::computePreDelayMs (float level) const
{
    return kMinPreDelayMs + (1.f - level) * (kMaxPreDelayMs - kMinPreDelayMs);
}
