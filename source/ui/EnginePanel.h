#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "KnobComponent.h"

// One sub-panel for a single engine (HAUNT VERB / SHIMMER / PITCH GHOST).
// Holds the engine knob(s) and an activity indicator glyph.
class EnginePanel : public juce::Component
{
public:
    enum class Engine { HauntVerb, Shimmer, PitchGhost };

    EnginePanel (Engine engine,
                 juce::AudioProcessorValueTreeState& apvts);

    void paint   (juce::Graphics& g) override;
    void resized () override;

    void setActive (bool isActive);

private:
    void drawHauntGlyph    (juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawShimmerGlyph  (juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawGhostGlyph    (juce::Graphics& g, juce::Rectangle<float> area) const;

    Engine engineType;
    bool   active       = false;
    float  glowIntensity = 0.f;  // 0–1, smoothly tracks active state

    std::unique_ptr<KnobComponent> primaryKnob;

    // Only for Shimmer: interval ComboBox
    std::unique_ptr<juce::ComboBox> intervalBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> intervalAttachment;
};
