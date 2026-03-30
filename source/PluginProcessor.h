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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void syncParametersFromAPVTS();
    void applyMix (juce::AudioBuffer<float>& wetBuffer,
                   const juce::AudioBuffer<float>& dryBuffer);

    EnvelopeFollower envelopeFollower;
    LiminalEngine    liminalEngine;
    RampSystem       rampSystem;
    ModMatrix        modMatrix;

    juce::AudioBuffer<float> dryBuffer;

    // Smoothed parameter values (audio-thread safe)
    juce::SmoothedValue<float> sThreshold, sSlew, sDepth, sMix;
    juce::SmoothedValue<float> sHaunt, sCrystallize, sPossession;

    // Atomic mirror for UI thread reads
    std::atomic<float> lastEnvelopeLevel { 0.f };
    std::atomic<float> lastBlendLevel    { 0.f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
