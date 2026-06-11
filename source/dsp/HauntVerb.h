#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Diffusion network + modulated cross-coupled stereo tank.
//
// Signal path per channel:
//   in → pre-delay → 4-stage allpass diffusion → tank
// The tank is a stereo figure-8: each channel's delay line feeds the other
// channel's input through a damping low-pass and a feedback gain mapped from
// the HAUNT amount (0.55 short breath → 0.95 near-infinite bloom). The tank
// delay times are slowly sine-modulated, which de-correlates repeats and
// removes the static metallic quality of an unmodulated loop.
class HauntVerb
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);

    // Renders the wet signal only (does not touch `in`); adds nothing when
    // blendFactor is 0 and the tank is empty.
    void renderWet (const juce::AudioBuffer<float>& in,
                    juce::AudioBuffer<float>& wet,
                    float blendFactor);

    void setHaunt         (float amount);       // 0.0–1.0, bloom intensity + decay
    void setEnvelopeLevel (float level);        // fed from EnvelopeFollower (normalized)

    bool isTankActive() const { return tankEnergy > 1e-7f; }

private:
    float computeHPFrequency (float envelopeLevel) const;
    float computePreDelayMs  (float envelopeLevel) const;

    static constexpr int kNumDiffusionStages = 4;
    static constexpr int kNumChannels        = 2;

    juce::dsp::DelayLine<float> diffusionLines[kNumChannels][kNumDiffusionStages];
    float diffusionCoeffs[kNumDiffusionStages] = { 0.52f, 0.42f, 0.33f, 0.25f };
    float diffFbLP[kNumChannels][kNumDiffusionStages] = {};

    // Pre-delay
    juce::dsp::DelayLine<float> preDelay[kNumChannels];

    // Late tank: one modulated delay per channel, cross-coupled feedback
    juce::dsp::DelayLine<float> tankLine[kNumChannels];
    float tankBaseDelaySamples[kNumChannels] = {};
    float tankDamp[kNumChannels]   = {};   // one-pole LP state in the loop
    float tankModPhase             = 0.f;
    float tankModInc               = 0.f;  // per-sample phase increment
    float tankEnergy               = 0.f;  // crude activity metric for early-out

    // High-pass that brightens as level drops
    juce::dsp::StateVariableTPTFilter<float> hpFilter;

    float hauntAmount    = 0.5f;
    float envelopeLevel  = 1.f;
    double sampleRate    = 44100.0;
};
