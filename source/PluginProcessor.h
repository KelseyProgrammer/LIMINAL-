#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/EnvelopeFollower.h"
#include "dsp/LiminalEngine.h"
#include "modulation/RampSystem.h"
#include "modulation/ModMatrix.h"

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool   acceptsMidi()        const override;
    bool   producesMidi()       const override;
    bool   isMidiEffect()       const override;
    double getTailLengthSeconds() const override;

    int               getNumPrograms()  override;
    int               getCurrentProgram() override;
    void              setCurrentProgram (int index) override;
    const juce::String getProgramName   (int index) override;
    void              changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData)           override;
    void setStateInformation (const void* data, int sizeInBytes)     override;

    // APVTS — public so the editor can attach sliders
    juce::AudioProcessorValueTreeState apvts;

    // Thread-safe reads for the UI
    float getEnvelopeLevel() const { return lastEnvelopeLevel.load(); }
    float getBlendLevel()    const { return lastBlendLevel.load(); }

    // Ramp A/B snapshot capture (call from message thread)
    void captureSnapshotA();
    void captureSnapshotB();

    // Kick the ramp system to animate in the current direction (audio-thread safe)
    void triggerRamp() { rampTriggerPending.store (true); }

    // Ramp position: 0.0 = snapshot A, 1.0 = snapshot B
    // Written by editor slider; also written by processBlock when latch animates
    std::atomic<float> rampPosition { 0.f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void syncParametersFromAPVTS();

    EnvelopeFollower envelopeFollower;
    LiminalEngine    liminalEngine;
    RampSystem       rampSystem;
    ModMatrix        modMatrix;

    // Smoothed parameter values (audio-thread safe)
    juce::SmoothedValue<float> sThreshold, sSlew, sDepth, sMix;
    juce::SmoothedValue<float> sHaunt, sCrystallize, sPossession;

    // Parameter snapshots for Ramp A/B morph
    struct Snapshot {
        float threshold = 0.3f, slew = 200.f, depth = 1.f;
        float haunt = 0.5f, crystallize = 0.f, possession = 0.5f;
        float mix = 1.f;
        int   interval = 0;
    };
    Snapshot snapshotA, snapshotB;
    bool hasSnapshotA = false, hasSnapshotB = false;

    // Atomic mirror for UI thread reads
    std::atomic<float> lastEnvelopeLevel  { 0.f };
    std::atomic<float> lastBlendLevel     { 0.f };
    std::atomic<bool>  rampTriggerPending { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
