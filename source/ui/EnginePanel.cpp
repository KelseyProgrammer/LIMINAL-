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
            primaryKnob = std::make_unique<KnobComponent> ("haunt", "HAUNT", apvts);
            addAndMakeVisible (*primaryKnob);
            break;

        case Engine::Shimmer:
            primaryKnob = std::make_unique<KnobComponent> ("crystallize", "CRYSTALLIZE", apvts);
            addAndMakeVisible (*primaryKnob);

            intervalBox = std::make_unique<juce::ComboBox>();
            intervalBox->addItemList ({ "Oct", "5th", "Oct+5th", "m2", "Tritone" }, 1);
            addAndMakeVisible (*intervalBox);
            intervalAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                apvts, "interval", *intervalBox);
            break;

        case Engine::PitchGhost:
            primaryKnob   = std::make_unique<KnobComponent> ("possession", "POSSESSION", apvts);
            addAndMakeVisible (*primaryKnob);
            break;
    }
}

void EnginePanel::setActive (bool isActive)
{
    active = isActive;
    repaint();
}

void EnginePanel::resized()
{
    auto area = getLocalBounds().reduced (6);
    const int glyphH = 24;
    const int knobH  = std::min (area.getWidth(), area.getHeight() - glyphH - 4);

    area.removeFromBottom (glyphH + 4);  // reserve bottom for glyph

    if (primaryKnob)
        primaryKnob->setBounds (area.removeFromTop (knobH));

    if (intervalBox)
        intervalBox->setBounds (area.removeFromTop (22));
}

void EnginePanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (LiminalLookAndFeel::PANEL_BORDER);
    g.drawRoundedRectangle (bounds.reduced (1.f), 4.f, 1.f);

    // Engine label
    const juce::String labels[] = { "HAUNT VERB", "SHIMMER", "PITCH GHOST" };
    g.setColour (LiminalLookAndFeel::GOLD);
    g.setFont (juce::Font (juce::FontOptions().withHeight (9.f).withStyle ("Bold")));
    g.drawText (labels[static_cast<int> (engineType)],
                bounds.reduced (4.f).removeFromTop (14.f),
                juce::Justification::centred);

    // Activity glyph at bottom
    const float glyphH   = 24.f;
    const auto glyphArea = bounds.reduced (4.f).removeFromBottom (glyphH);

    switch (engineType)
    {
        case Engine::HauntVerb:   drawHauntGlyph   (g, glyphArea); break;
        case Engine::Shimmer:     drawShimmerGlyph (g, glyphArea); break;
        case Engine::PitchGhost:  drawGhostGlyph   (g, glyphArea); break;
    }
}

void EnginePanel::drawHauntGlyph (juce::Graphics& g, juce::Rectangle<float> area) const
{
    // Crescent moon ☽
    const juce::Colour col = active ? LiminalLookAndFeel::ICE_BLUE
                                    : LiminalLookAndFeel::TEXT_DIM;
    if (active)
    {
        // Glow
        juce::ColourGradient glow (col.withAlpha (0.25f),
                                   area.getCentreX(), area.getCentreY(),
                                   col.withAlpha (0.f),
                                   area.getCentreX() + area.getWidth() * 0.5f, area.getCentreY(),
                                   true);
        g.setGradientFill (glow);
        g.fillEllipse (area.reduced (2.f));
    }

    const float cx = area.getCentreX();
    const float cy = area.getCentreY();
    const float r  = area.getHeight() * 0.38f;

    juce::Path crescent;
    crescent.addEllipse (cx - r, cy - r, r * 2.f, r * 2.f);

    // Clip with offset circle to create crescent
    juce::Path clip;
    clip.addEllipse (cx - r * 0.35f, cy - r, r * 2.f, r * 2.f);

    juce::PathStrokeType stroke (1.5f);
    juce::Path stroked;
    stroke.createStrokedPath (stroked, crescent);

    // Draw full circle then overdraw right side with background to fake crescent
    g.setColour (col);
    g.strokePath (crescent, stroke);

    // Mask the right portion: draw arc instead
    g.setColour (LiminalLookAndFeel::COBALT);
    g.fillPath (clip);

    g.setColour (col);
    // Redraw left crescent arc
    juce::Path arc;
    arc.addCentredArc (cx, cy, r, r, 0.f,
                       juce::MathConstants<float>::pi * 0.6f,
                       juce::MathConstants<float>::pi * 1.4f, true);
    g.strokePath (arc, stroke);
}

void EnginePanel::drawShimmerGlyph (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const juce::Colour col = active ? LiminalLookAndFeel::GOLD
                                    : LiminalLookAndFeel::GOLD_DIM;
    if (active)
    {
        juce::ColourGradient glow (col.withAlpha (0.25f),
                                   area.getCentreX(), area.getCentreY(),
                                   col.withAlpha (0.f),
                                   area.getCentreX() + area.getWidth() * 0.5f, area.getCentreY(),
                                   true);
        g.setGradientFill (glow);
        g.fillEllipse (area.reduced (2.f));
    }

    // Six-point star ✦
    const float cx = area.getCentreX();
    const float cy = area.getCentreY();
    const float r  = area.getHeight() * 0.38f;

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

    g.setColour (col);
    g.strokePath (star, juce::PathStrokeType (1.5f));
}

void EnginePanel::drawGhostGlyph (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const juce::Colour col = active ? LiminalLookAndFeel::GHOST_WHITE
                                    : LiminalLookAndFeel::TEXT_DIM;
    if (active)
    {
        juce::ColourGradient glow (col.withAlpha (0.2f),
                                   area.getCentreX(), area.getCentreY(),
                                   col.withAlpha (0.f),
                                   area.getCentreX() + area.getWidth() * 0.5f, area.getCentreY(),
                                   true);
        g.setGradientFill (glow);
        g.fillEllipse (area.reduced (2.f));
    }

    // Four-point star ✧
    const float cx = area.getCentreX();
    const float cy = area.getCentreY();
    const float r  = area.getHeight() * 0.38f;

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

    g.setColour (col);
    g.strokePath (star, juce::PathStrokeType (1.5f));
}
