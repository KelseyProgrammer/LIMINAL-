#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"

#include "ui/LiminalLookAndFeel.h"
#include "ui/ThresholdDisplay.h"
#include "ui/EnginePanel.h"
#include "ui/KnobComponent.h"

// The editor scales: all controls live on a fixed 950x668 content component
// (matching the background artwork 1:1) which is transform-scaled to the
// window size. Aspect ratio is locked; the chosen size persists in the
// plugin state.
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

    void paintContent     (juce::Graphics& g);
    void paintContentOver (juce::Graphics& g);
    void layoutContent();

    static constexpr int kContentW = 950;
    static constexpr int kContentH = 668;

    PluginProcessor& processorRef;

    LiminalLookAndFeel lookAndFeel;

    // Fixed-size canvas holding every control; scaled via AffineTransform
    struct ContentComponent : juce::Component
    {
        explicit ContentComponent (PluginEditor& e) : editor (e) {}
        void paint             (juce::Graphics& g) override { editor.paintContent (g); }
        void paintOverChildren (juce::Graphics& g) override { editor.paintContentOver (g); }
        PluginEditor& editor;
    };
    ContentComponent content { *this };

    juce::Image backgroundImage;

    // Central display
    ThresholdDisplay thresholdDisplay;
    std::unique_ptr<juce::ParameterAttachment> thresholdDragAttachment;

    // Five main knobs (top row)
    KnobComponent knobThreshold, knobSlew, knobDepth, knobTone, knobMix;

    // Toggles
    juce::ToggleButton invertButton    { "INV" };
    juce::ToggleButton sidechainButton { "SC" };

    // Three engine panels
    EnginePanel hauntPanel, shimmerPanel, ghostPanel;

    // Modulation row
    KnobComponent knobLfoRate, knobLfoToDepth, knobLfoToHaunt, knobLfoToCrystallize, knobEnvToDepth;
    juce::ComboBox lfoSyncBox;

    // Ramp strip (bottom)
    juce::TextButton   rampAButton { "A" };
    juce::TextButton   rampBButton { "B" };
    juce::Slider       rampSlider;
    juce::ToggleButton latchButton    { "LATCH" };
    juce::ToggleButton autoRampButton { "AUTO" };
    juce::ComboBox     rampSyncBox;
    KnobComponent      knobRampTime;

    // APVTS attachments (must outlive widgets)
    juce::AudioProcessorValueTreeState::ButtonAttachment invertAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment latchAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment autoRampAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment sidechainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoSyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rampSyncAttachment;

    // Border flash when the envelope crosses below the threshold
    float borderFlash  = 0.f;
    float prevEnvelope = 1.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
