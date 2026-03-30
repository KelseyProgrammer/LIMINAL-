#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// A labelled rotary slider with APVTS attachment.
class KnobComponent : public juce::Component
{
public:
    KnobComponent (const juce::String& paramID,
                   const juce::String& label,
                   juce::AudioProcessorValueTreeState& apvts);

    void resized() override;
    void paint (juce::Graphics& g) override;

    juce::Slider slider;

private:
    juce::Label  labelComponent;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    juce::String labelText;
};
