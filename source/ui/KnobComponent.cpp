#include "KnobComponent.h"

KnobComponent::KnobComponent (const juce::String& paramID,
                               const juce::String& label,
                               juce::AudioProcessorValueTreeState& apvts)
    : labelText (label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (slider);

    labelComponent.setText (label, juce::dontSendNotification);
    labelComponent.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (labelComponent);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, paramID, slider);
}

void KnobComponent::resized()
{
    auto area = getLocalBounds();
    const int labelH = 16;
    labelComponent.setBounds (area.removeFromBottom (labelH));
    slider.setBounds (area);
}

void KnobComponent::paint (juce::Graphics& /*g*/)
{
}
