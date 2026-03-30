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
            primaryKnob = std::make_unique<KnobComponent> ("possession", "POSSESSION", apvts);
            addAndMakeVisible (*primaryKnob);
            break;
    }
}

void EnginePanel::setActive (bool isActive)
{
    active = isActive;
    // Smooth glow transition: ~15% per call at 30fps ≈ 0.5s fade
    const float target = isActive ? 1.f : 0.f;
    glowIntensity = glowIntensity + (target - glowIntensity) * 0.15f;
    repaint();
}

void EnginePanel::resized()
{
    auto area = getLocalBounds().reduced (6);
    const int labelH = 14;
    const int glyphH = 26;

    area.removeFromTop (labelH + 2);   // engine label
    area.removeFromBottom (glyphH + 4); // glyph strip

    if (primaryKnob)
        primaryKnob->setBounds (area.removeFromTop (std::min (area.getWidth(), area.getHeight())));

    if (intervalBox)
        intervalBox->setBounds (area.removeFromTop (22));
}

void EnginePanel::paint (juce::Graphics& g)
{
    const auto bounds  = getLocalBounds().toFloat();
    const auto reduced = bounds.reduced (1.f);

    // ── Dark navy fill ────────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xff0d1535));
    g.fillRoundedRectangle (reduced, 5.f);

    // ── Per-engine activation glow overlay ───────────────────────────────────
    if (glowIntensity > 0.005f)
    {
        const juce::Colour engineGlowColors[3] = {
            LiminalLookAndFeel::ICE_BLUE,
            LiminalLookAndFeel::GOLD,
            LiminalLookAndFeel::GHOST_WHITE
        };
        const juce::Colour glowCol = engineGlowColors[static_cast<int> (engineType)];
        juce::ColourGradient glow (glowCol.withAlpha (glowIntensity * 0.12f),
                                   bounds.getCentreX(), bounds.getCentreY(),
                                   glowCol.withAlpha (0.f),
                                   bounds.getRight(), bounds.getCentreY(),
                                   true);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (reduced, 5.f);
    }

    // ── Gold border ───────────────────────────────────────────────────────────
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.65f + glowIntensity * 0.35f));
    g.drawRoundedRectangle (reduced, 5.f, 1.f);

    // ── Corner stars ✧ (top-left and top-right) ───────────────────────────────
    const float starAlpha = 0.4f + glowIntensity * 0.5f;
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (starAlpha));
    const float sr = 4.f;
    const float positions[2][2] = { { reduced.getX() + 7.f, reduced.getY() + 7.f },
                                     { reduced.getRight() - 7.f, reduced.getY() + 7.f } };
    for (auto [sx, sy] : positions)
    {
        for (int i = 0; i < 4; ++i)
        {
            const float a = i * juce::MathConstants<float>::halfPi;
            g.drawLine (sx, sy, sx + sr * std::cos (a), sy + sr * std::sin (a), 0.75f);
        }
    }

    // ── Engine label ──────────────────────────────────────────────────────────
    const juce::String labels[] = { "HAUNT VERB", "SHIMMER", "PITCH GHOST" };
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.8f + glowIntensity * 0.2f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (9.f).withStyle ("Bold")));
    g.drawText (labels[static_cast<int> (engineType)],
                bounds.reduced (4.f).removeFromTop (14.f),
                juce::Justification::centred);

    // ── Glyph strip (darker inset at bottom) ─────────────────────────────────
    const float glyphH    = 26.f;
    const auto glyphStrip = bounds.reduced (4.f).removeFromBottom (glyphH);

    // Darker background behind glyph
    g.setColour (juce::Colour (0x40000000));
    g.fillRoundedRectangle (glyphStrip, 3.f);

    switch (engineType)
    {
        case Engine::HauntVerb:   drawHauntGlyph   (g, glyphStrip); break;
        case Engine::Shimmer:     drawShimmerGlyph (g, glyphStrip); break;
        case Engine::PitchGhost:  drawGhostGlyph   (g, glyphStrip); break;
    }
}

void EnginePanel::drawHauntGlyph (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const juce::Colour col = active ? LiminalLookAndFeel::ICE_BLUE
                                    : LiminalLookAndFeel::TEXT_DIM;
    if (glowIntensity > 0.01f)
    {
        juce::ColourGradient glow (col.withAlpha (glowIntensity * 0.3f),
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

    g.setColour (col);
    juce::Path arc;
    arc.addCentredArc (cx, cy, r, r, 0.f,
                       juce::MathConstants<float>::pi * 0.55f,
                       juce::MathConstants<float>::pi * 1.45f, true);
    g.strokePath (arc, juce::PathStrokeType (1.5f));
}

void EnginePanel::drawShimmerGlyph (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const juce::Colour col = active ? LiminalLookAndFeel::GOLD : LiminalLookAndFeel::GOLD_DIM;

    if (glowIntensity > 0.01f)
    {
        juce::ColourGradient glow (col.withAlpha (glowIntensity * 0.3f),
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
    if (glowIntensity > 0.01f)
    {
        juce::ColourGradient glow (col.withAlpha (glowIntensity * 0.25f),
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
