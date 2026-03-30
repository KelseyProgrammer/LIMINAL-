#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"

#include "ui/LiminalLookAndFeel.h"
#include "ui/ThresholdDisplay.h"
#include "ui/EnginePanel.h"
#include "ui/KnobComponent.h"

class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    void timerCallback() override;

    PluginProcessor& processorRef;

    LiminalLookAndFeel lookAndFeel;

    // Central display
    ThresholdDisplay thresholdDisplay;

    // Five main knobs (top row)
    KnobComponent knobThreshold, knobSlew, knobDepth, knobTone, knobMix;

    // Three engine panels
    EnginePanel hauntPanel, shimmerPanel, ghostPanel;

    // Ramp strip (bottom)
    juce::Slider     rampSlider;
    juce::ToggleButton latchButton { "LATCH" };
    KnobComponent    knobRampTime;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
