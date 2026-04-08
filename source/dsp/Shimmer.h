#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Granular pitch shifter voice using two overlapping grains with distance-based windowing.
// The read head starts kGrainSamples behind the write head to avoid reading unwritten data.
struct PitchShifterVoice
{
    // Larger grain size = smoother transitions, fewer audible grain boundaries
    static constexpr int kGrainSamples = 4096;

    juce::AudioBuffer<float> grainBuffer;
    int   writePos   = 0;
    float readPos    = 0.f;
    float pitchRatio = 1.f;

    void prepare (const juce::dsp::ProcessSpec& spec);

    // Push a new input sample and return the pitch-shifted output for one channel
    float processSample (float input, int channel);

    void setPitchRatio (float ratio) { pitchRatio = ratio; }
};

class Shimmer
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer, float blendFactor);

    void setCrystallize (float amount);      // 0.0–1.0
    void setInterval    (int semitones);     // interval for voice1
    void setFeedback    (float feedback);    // 0.0–0.95

private:
    static float semitonesToRatio (int semitones);

    PitchShifterVoice voice1[2], voice2[2]; // [channel]

    juce::AudioBuffer<float> feedbackBuffer;

    float crystallizeAmount = 0.f;
    float feedbackLevel     = 0.15f; // conservative — prevents metallic buildup
    int   intervalSemitones = 12;

    // Frozen output for crystallize=1 hold
    juce::AudioBuffer<float> frozenBuffer;

    // One-pole LP per channel to smooth grain boundary artifacts
    float lpState[2] = {};

    double sampleRate = 44100.0;
};
