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

// Pitch-shifted shimmer cascade + CRYSTALLIZE freeze.
//
// The pitch shifters sit inside a ~340ms damped feedback delay, so each
// repeat climbs the interval again and washes upward — the classic shimmer
// cascade (previously the feedback was one block long, which only produced
// a static harmonizer doubling).
//
// CRYSTALLIZE: the shimmer output is continuously recorded into a ring;
// when the knob rises past the trigger point, the last ~2s are latched and
// looped with boundary crossfades. At 1.0 the loop sustains forever
// (independent of blend — the drone persists while you play); below 1.0 it
// decays at a rate set by the knob.
class Shimmer
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);

    // Renders the wet signal only (does not touch `in`).
    void renderWet (const juce::AudioBuffer<float>& in,
                    juce::AudioBuffer<float>& wet,
                    float blendFactor);

    void setCrystallize (float amount);      // 0.0–1.0
    void setInterval    (int semitones);     // interval for voice1
    void setFeedback    (float feedback);    // 0.0–0.75

    bool isFrozen() const { return frozenActive; }

private:
    static float semitonesToRatio (float semitones);
    void captureFreeze();

    PitchShifterVoice voice1[2], voice2[2]; // [channel]

    // Cascade feedback path: damped delay so repeats climb and bloom
    juce::dsp::DelayLine<float> cascadeDelay;
    float cascadeDamp[2] = {};

    float crystallizeAmount = 0.f;
    float prevCrystallize   = 0.f;
    float feedbackLevel     = 0.45f;
    int   intervalSemitones = 12;

    // ── Freeze (CRYSTALLIZE) ─────────────────────────────────────────────────
    static constexpr int kFreezeRingSize = 1 << 17;   // ~3s at 44.1kHz, power of 2
    juce::AudioBuffer<float> freezeRing;              // continuously records shimmer out
    int  freezeWritePos = 0;

    juce::AudioBuffer<float> frozenLoop;              // latched snapshot, looped
    int   frozenLength  = 0;
    float frozenReadPos = 0.f;
    float frozenEnv     = 0.f;
    bool  frozenActive  = false;

    // One-pole LP per channel to smooth grain boundary artifacts
    float lpState[2] = {};

    double sampleRate = 44100.0;
};
