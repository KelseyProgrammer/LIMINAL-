#include "EnginePanel.h"
#include "LiminalLookAndFeel.h"

#include <cmath>

EnginePanel::EnginePanel (Engine engine,
                           juce::AudioProcessorValueTreeState& apvts)
    : engineType (engine)
{
    switch (engine)
    {
        case Engine::HauntVerb:
            primaryKnob = std::make_unique<KnobComponent> ("haunt", "HAUNT", apvts, false);
            addAndMakeVisible (*primaryKnob);
            break;

        case Engine::Shimmer:
            primaryKnob = std::make_unique<KnobComponent> ("crystallize", "CRYSTALLIZE", apvts, false);
            addAndMakeVisible (*primaryKnob);

            intervalBox = std::make_unique<juce::ComboBox>();
            intervalBox->addItemList ({ "Oct", "5th", "Oct+5th", "m2", "Tritone" }, 1);
            addAndMakeVisible (*intervalBox);
            intervalAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                apvts, "interval", *intervalBox);
            break;

        case Engine::PitchGhost:
            primaryKnob = std::make_unique<KnobComponent> ("possession", "POSSESSION", apvts, false);
            addAndMakeVisible (*primaryKnob);

            driftKnob = std::make_unique<KnobComponent> ("driftRate", "DRIFT", apvts, false);
            addAndMakeVisible (*driftKnob);

            driftDirBox = std::make_unique<juce::ComboBox>();
            driftDirBox->addItemList ({ "Down", "Wander", "Up" }, 1);
            addAndMakeVisible (*driftDirBox);
            driftDirAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                apvts, "driftDir", *driftDirBox);
            break;
    }
}

void EnginePanel::setActive (bool isActive)
{
    active = isActive;
    // Smooth glow transition: ~15% per call at 30fps ≈ 0.5s fade
    const float target = isActive ? 1.f : 0.f;
    const float next   = glowIntensity + (target - glowIntensity) * 0.15f;

    // Only repaint while the glow is actually moving or alive — idle panels
    // are pure background and repainting them costs a full image blit
    if (std::abs (next - glowIntensity) > 0.001f || next > 0.005f)
        repaint();

    glowIntensity = next;
}

void EnginePanel::setGlowLevel (float blend)
{
    blendLevel = blend;
}

juce::Point<int> EnginePanel::knobCentreInPanel() const
{
    // Painted knob centres (absolute: 167/472/779 at y=401) relative to the
    // panel bounds set in PluginEditor::resized().
    switch (engineType)
    {
        case Engine::HauntVerb:  return { 157, 46 };
        case Engine::Shimmer:    return { 152, 46 };
        case Engine::PitchGhost: return { 144, 46 };
    }
    return { getWidth() / 2, 46 };
}

void EnginePanel::resized()
{
    const auto kc = knobCentreInPanel();
    const int knobSize = 52;
    if (primaryKnob)
        primaryKnob->setBounds (kc.x - knobSize / 2, kc.y - knobSize / 2, knobSize, knobSize);

    // Interval selector tucked into the lower-right of the Shimmer panel
    if (intervalBox)
        intervalBox->setBounds (getWidth() - 86, getHeight() - 28, 76, 20);

    // Drift controls in the PitchGhost panel: small rate knob upper-left,
    // direction selector lower-left
    if (driftKnob)
        driftKnob->setBounds (40, 30, 30, 30);
    if (driftDirBox)
        driftDirBox->setBounds (12, getHeight() - 28, 80, 20);
}

void EnginePanel::paint (juce::Graphics& g)
{
    // The panel frame and labels are part of the background artwork —
    // draw only additive light on top of it.
    if (glowIntensity < 0.005f)
        return;

    const auto bounds = getLocalBounds().toFloat();

    const juce::Colour engineColors[3] = {
        LiminalLookAndFeel::ICE_BLUE,
        LiminalLookAndFeel::GOLD,
        LiminalLookAndFeel::GHOST_WHITE
    };
    const juce::Colour ec = engineColors[static_cast<int> (engineType)];

    // Soft activation wash across the panel, strongest near the knob
    const auto kc = knobCentreInPanel().toFloat();
    const float washAlpha = glowIntensity * (0.05f + blendLevel * 0.10f);
    juce::ColourGradient wash (ec.withAlpha (washAlpha),
                               kc.x, kc.y,
                               ec.withAlpha (0.f),
                               kc.x + bounds.getWidth() * 0.55f, kc.y,
                               true);
    g.setGradientFill (wash);
    g.fillRoundedRectangle (bounds.reduced (3.f), 8.f);

    // Glyph pulse over the painted icon below the engine label
    const float gx = kc.x;
    const float gy = bounds.getHeight() - 23.f;
    const float gr = 7.f;
    const float glyphAlpha = glowIntensity * (0.25f + blendLevel * 0.75f);

    juce::ColourGradient halo (ec.withAlpha (glyphAlpha * 0.35f), gx, gy,
                               ec.withAlpha (0.f), gx + gr * 2.6f, gy, true);
    g.setGradientFill (halo);
    g.fillEllipse (gx - gr * 2.6f, gy - gr * 2.6f, gr * 5.2f, gr * 5.2f);

    g.setColour (ec.withAlpha (glyphAlpha));
    switch (engineType)
    {
        case Engine::HauntVerb:   drawHauntGlyph   (g, gx, gy, gr); break;
        case Engine::Shimmer:     drawShimmerGlyph (g, gx, gy, gr); break;
        case Engine::PitchGhost:  drawGhostGlyph   (g, gx, gy, gr); break;
    }
}

void EnginePanel::drawHauntGlyph (juce::Graphics& g, float cx, float cy, float r) const
{
    juce::Path arc;
    arc.addCentredArc (cx, cy, r, r, 0.f,
                       juce::MathConstants<float>::pi * 0.55f,
                       juce::MathConstants<float>::pi * 1.45f, true);
    g.strokePath (arc, juce::PathStrokeType (1.5f));
}

void EnginePanel::drawShimmerGlyph (juce::Graphics& g, float cx, float cy, float r) const
{
    juce::Path star;
    const float step     = juce::MathConstants<float>::twoPi / 6.f;
    const float startAng = -juce::MathConstants<float>::halfPi;

    for (int i = 0; i < 6; ++i)
    {
        const float outerAng = startAng + i * step;
        const float innerAng = outerAng + step * 0.5f;
        const juce::Point<float> outer { cx + r * std::cos (outerAng),
                                          cy + r * std::sin (outerAng) };
        const juce::Point<float> inner { cx + r * 0.4f * std::cos (innerAng),
                                          cy + r * 0.4f * std::sin (innerAng) };
        if (i == 0) star.startNewSubPath (outer);
        else        star.lineTo (outer);
        star.lineTo (inner);
    }
    star.closeSubPath();
    g.strokePath (star, juce::PathStrokeType (1.5f));
}

void EnginePanel::drawGhostGlyph (juce::Graphics& g, float cx, float cy, float r) const
{
    juce::Path star;
    const float step     = juce::MathConstants<float>::twoPi / 4.f;
    const float startAng = -juce::MathConstants<float>::halfPi;

    for (int i = 0; i < 4; ++i)
    {
        const float outerAng = startAng + i * step;
        const float innerAng = outerAng + step * 0.5f;
        const juce::Point<float> outer { cx + r * std::cos (outerAng),
                                          cy + r * std::sin (outerAng) };
        const juce::Point<float> inner { cx + r * 0.25f * std::cos (innerAng),
                                          cy + r * 0.25f * std::sin (innerAng) };
        if (i == 0) star.startNewSubPath (outer);
        else        star.lineTo (outer);
        star.lineTo (inner);
    }
    star.closeSubPath();
    g.strokePath (star, juce::PathStrokeType (1.5f));
}
