#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// A rotary slider with APVTS attachment. When showLabel is false the knob
// fills its whole bounds and relies on a label painted in the background
// artwork; the parameter value still appears in a popup while dragging.
class KnobComponent : public juce::Component
{
public:
    KnobComponent (const juce::String& paramID,
                   const juce::String& label,
                   juce::AudioProcessorValueTreeState& apvts,
                   bool showLabel = true);

    void resized() override;
    void paint (juce::Graphics& g) override;

    juce::Slider slider;

private:
    juce::Label  labelComponent;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    juce::String labelText;
    bool         labelVisible;
};
