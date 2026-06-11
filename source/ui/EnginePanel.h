#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "KnobComponent.h"

// Overlay for one engine panel (HAUNT VERB / SHIMMER / PITCH GHOST).
//
// The panel frame, label and ornaments are painted in the background
// artwork; this component sits exactly over a painted panel and contributes
// only the live parts: the real knob, an activation glow wash, and a glowing
// glyph pulse over the painted icon. The Shimmer panel also hosts the
// interval selector.
class EnginePanel : public juce::Component
{
public:
    enum class Engine { HauntVerb, Shimmer, PitchGhost };

    EnginePanel (Engine engine,
                 juce::AudioProcessorValueTreeState& apvts);

    void paint   (juce::Graphics& g) override;
    void resized () override;

    void setActive    (bool isActive);
    void setGlowLevel (float blend);   // 0–1, engine blend from the processor

private:
    void drawHauntGlyph    (juce::Graphics& g, float cx, float cy, float r) const;
    void drawShimmerGlyph  (juce::Graphics& g, float cx, float cy, float r) const;
    void drawGhostGlyph    (juce::Graphics& g, float cx, float cy, float r) const;

    // Knob centre within this panel, measured from the painted artwork
    juce::Point<int> knobCentreInPanel() const;

    Engine engineType;
    bool   active        = false;
    float  glowIntensity = 0.f;  // 0–1, smoothly tracks active state
    float  blendLevel    = 0.f;

    std::unique_ptr<KnobComponent> primaryKnob;

    // Only for Shimmer: interval ComboBox
    std::unique_ptr<juce::ComboBox> intervalBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> intervalAttachment;
};
