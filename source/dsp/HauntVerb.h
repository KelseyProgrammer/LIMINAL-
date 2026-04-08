#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class HauntVerb
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer, float blendFactor);

    void setHaunt         (float amount);       // 0.0–1.0, bloom intensity
    void setEnvelopeLevel (float level);        // fed from EnvelopeFollower

private:
    float computeHPFrequency (float envelopeLevel) const;
    float computePreDelayMs  (float envelopeLevel) const;

    // 4-stage allpass diffusion per channel (stereo = 2 channels)
    static constexpr int kNumDiffusionStages = 4;
    static constexpr int kNumChannels        = 2;

    juce::dsp::DelayLine<float> diffusionLines[kNumChannels][kNumDiffusionStages];
    // Softer coefficients reduce metallic ringing in the diffusion network
    float diffusionCoeffs[kNumDiffusionStages] = { 0.52f, 0.42f, 0.33f, 0.25f };
    // One-pole LP state in the allpass feedback path — prevents high-frequency buildup
    float diffFbLP[kNumChannels][kNumDiffusionStages] = {};

    // Pre-delay
    juce::dsp::DelayLine<float> preDelay[kNumChannels];

    // High-pass that brightens as level drops
    juce::dsp::StateVariableTPTFilter<float> hpFilter;

    float hauntAmount    = 0.5f;
    float envelopeLevel  = 1.f;
    double sampleRate    = 44100.0;
};
