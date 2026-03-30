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
    float diffusionCoeffs[kNumDiffusionStages] = { 0.7f, 0.6f, 0.5f, 0.4f };

    // Pre-delay
    juce::dsp::DelayLine<float> preDelay[kNumChannels];

    // High-pass that brightens as level drops
    juce::dsp::StateVariableTPTFilter<float> hpFilter;

    float hauntAmount    = 0.5f;
    float envelopeLevel  = 1.f;
    double sampleRate    = 44100.0;
};
