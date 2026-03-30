#include "PluginEditor.h"
#include "ui/LiminalLookAndFeel.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      knobThreshold ("threshold", "THRESHOLD", p.apvts),
      knobSlew      ("slew",      "SLEW",      p.apvts),
      knobDepth     ("depth",     "DEPTH",     p.apvts),
      knobTone      ("tone",      "TONE",      p.apvts),
      knobMix       ("mix",       "MIX",       p.apvts),
      hauntPanel    (EnginePanel::Engine::HauntVerb,  p.apvts),
      shimmerPanel  (EnginePanel::Engine::Shimmer,    p.apvts),
      ghostPanel    (EnginePanel::Engine::PitchGhost, p.apvts),
      knobLfoRate            ("lfoRate",          "LFO RATE",    p.apvts),
      knobLfoToDepth         ("lfoToDepth",       "LFO>DEPTH",   p.apvts),
      knobLfoToHaunt         ("lfoToHaunt",       "LFO>HAUNT",   p.apvts),
      knobLfoToCrystallize   ("lfoToCrystallize", "LFO>CRYST",   p.apvts),
      knobEnvToDepth         ("envToDepth",       "ENV>DEPTH",   p.apvts),
      knobRampTime           ("rampTime",         "TIME",        p.apvts),
      invertAttachment (p.apvts, "invertMode",  invertButton),
      latchAttachment  (p.apvts, "latch",       latchButton)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (thresholdDisplay);

    addAndMakeVisible (knobThreshold);
    addAndMakeVisible (knobSlew);
    addAndMakeVisible (knobDepth);
    addAndMakeVisible (knobTone);
    addAndMakeVisible (knobMix);
    addAndMakeVisible (invertButton);

    addAndMakeVisible (hauntPanel);
    addAndMakeVisible (shimmerPanel);
    addAndMakeVisible (ghostPanel);

    addAndMakeVisible (knobLfoRate);
    addAndMakeVisible (knobLfoToDepth);
    addAndMakeVisible (knobLfoToHaunt);
    addAndMakeVisible (knobLfoToCrystallize);
    addAndMakeVisible (knobEnvToDepth);

    rampAButton.onClick = [this] { processorRef.captureSnapshotA(); };
    rampBButton.onClick = [this] { processorRef.captureSnapshotB(); };
    addAndMakeVisible (rampAButton);
    addAndMakeVisible (rampBButton);

    rampSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    rampSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    rampSlider.setRange (0.0, 1.0);
    rampSlider.onValueChange = [this] {
        processorRef.rampPosition.store (static_cast<float> (rampSlider.getValue()));
    };
    addAndMakeVisible (rampSlider);

    latchButton.onClick = [this] { processorRef.triggerRamp(); };
    addAndMakeVisible (latchButton);
    addAndMakeVisible (knobRampTime);

    setSize (800, 568);
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
    stopTimer();
}

void PluginEditor::timerCallback()
{
    const float blend     = processorRef.getBlendLevel();
    const float envelope  = processorRef.getEnvelopeLevel();
    const float threshold = processorRef.apvts.getRawParameterValue ("threshold")->load();

    thresholdDisplay.setBlendLevel     (blend);
    thresholdDisplay.setEnvelopeLevel  (envelope);
    thresholdDisplay.setThresholdValue (threshold);

    const bool active = blend > 0.01f;
    hauntPanel  .setActive (active);
    shimmerPanel.setActive (active);
    ghostPanel  .setActive (active);

    thresholdDisplay.setHauntActive      (active);
    thresholdDisplay.setShimmerActive    (active);
    thresholdDisplay.setPitchGhostActive (active);

    // Feed LFO phase to look-and-feel for orbiting dot on mod knobs
    lookAndFeel.setLFOPhase (processorRef.lfoPhase.load());

    rampSlider.setValue (static_cast<double> (processorRef.rampPosition.load()),
                         juce::dontSendNotification);
}

// ── Background helpers ─────────────────────────────────────────────────────────

void PluginEditor::draw4PointStar (juce::Graphics& g, float cx, float cy, float r)
{
    juce::Path star;
    for (int i = 0; i < 4; ++i)
    {
        const float outerA = i * juce::MathConstants<float>::halfPi
                             - juce::MathConstants<float>::halfPi;
        const float innerA = outerA + juce::MathConstants<float>::pi / 4.f;
        const juce::Point<float> outer { cx + r * std::cos (outerA),
                                          cy + r * std::sin (outerA) };
        const juce::Point<float> inner { cx + r * 0.28f * std::cos (innerA),
                                          cy + r * 0.28f * std::sin (innerA) };
        if (i == 0) star.startNewSubPath (outer);
        else        star.lineTo (outer);
        star.lineTo (inner);
    }
    star.closeSubPath();
    g.strokePath (star, juce::PathStrokeType (1.f));
}

void PluginEditor::drawCrescent (juce::Graphics& g, float cx, float cy, float r)
{
    juce::Path crescent;
    crescent.addCentredArc (cx, cy, r, r, 0.f,
                             juce::MathConstants<float>::pi * 0.6f,
                             juce::MathConstants<float>::pi * 1.4f, true);
    g.strokePath (crescent, juce::PathStrokeType (1.2f));
}

void PluginEditor::drawFiligreeBorder (juce::Graphics& g)
{
    const float W      = static_cast<float> (getWidth());
    const float H      = static_cast<float> (getHeight());
    const float inset  = 6.f;
    const float cornerR = 7.f;

    // Outer border line
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.85f));
    g.drawRoundedRectangle (inset, inset,
                             W - inset * 2.f, H - inset * 2.f, cornerR, 1.5f);

    // Inner border line
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.28f));
    g.drawRoundedRectangle (inset + 5.f, inset + 5.f,
                             W - (inset + 5.f) * 2.f, H - (inset + 5.f) * 2.f,
                             cornerR - 2.f, 0.75f);

    // ── Corner ornaments ─────────────────────────────────────────────────────
    const float co = 22.f;
    struct CornerDesc { float ox, oy, aStart, aEnd; };
    const CornerDesc corners[4] = {
        { inset,     inset,     juce::MathConstants<float>::pi,
                                juce::MathConstants<float>::pi * 1.5f },
        { W - inset, inset,     juce::MathConstants<float>::pi * 1.5f,
                                juce::MathConstants<float>::twoPi },
        { inset,     H - inset, juce::MathConstants<float>::halfPi,
                                juce::MathConstants<float>::pi },
        { W - inset, H - inset, 0.f,
                                juce::MathConstants<float>::halfPi }
    };

    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.7f));
    for (const auto& c : corners)
    {
        juce::Path arc;
        arc.addCentredArc (c.ox, c.oy, co, co, 0.f, c.aStart, c.aEnd, true);
        g.strokePath (arc, juce::PathStrokeType (1.f));

        const float midAngle = (c.aStart + c.aEnd) * 0.5f;
        draw4PointStar (g,
                        c.ox + co * std::cos (midAngle),
                        c.oy + co * std::sin (midAngle),
                        4.f);
    }

    // ── Top center ornament ───────────────────────────────────────────────────
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.7f));
    draw4PointStar (g, W * 0.5f, inset, 6.f);
    g.drawLine (W * 0.5f - 30.f, inset, W * 0.5f - 9.f,  inset, 1.f);
    g.drawLine (W * 0.5f + 9.f,  inset, W * 0.5f + 30.f, inset, 1.f);

    // ── Bottom center ornament ────────────────────────────────────────────────
    draw4PointStar (g, W * 0.5f, H - inset, 6.f);
    g.drawLine (W * 0.5f - 30.f, H - inset, W * 0.5f - 9.f,  H - inset, 1.f);
    g.drawLine (W * 0.5f + 9.f,  H - inset, W * 0.5f + 30.f, H - inset, 1.f);

    // ── Mid-side diamonds ─────────────────────────────────────────────────────
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.5f));
    const float dSize = 4.f;
    for (float dx : { inset, W - inset })
    {
        juce::Path d;
        d.startNewSubPath (dx,        H * 0.5f - dSize);
        d.lineTo          (dx + dSize, H * 0.5f);
        d.lineTo          (dx,        H * 0.5f + dSize);
        d.lineTo          (dx - dSize, H * 0.5f);
        d.closeSubPath();
        g.strokePath (d, juce::PathStrokeType (0.75f));
    }
}

void PluginEditor::drawCelestialDecorations (juce::Graphics& g)
{
    g.setColour (LiminalLookAndFeel::GOLD_DIM.withAlpha (0.6f));

    // Crescent moons flanking knobs and near borders
    struct Moon { float x, y, r; };
    const Moon moons[] = {
        { 55.f,  300.f, 8.f },
        { 160.f, 275.f, 6.f },
        { 340.f, 260.f, 7.f },
        { 460.f, 275.f, 6.f },
        { 640.f, 295.f, 7.f },
        { 755.f, 300.f, 6.f },
        { 90.f,  460.f, 5.f },
        { 700.f, 460.f, 5.f },
    };
    for (const auto& m : moons)
        drawCrescent (g, m.x, m.y, m.r);

    // Small 4-point stars scattered in open areas
    struct Star { float x, y, r; };
    const Star stars[] = {
        { 185.f, 220.f, 4.f  },
        { 620.f, 195.f, 3.5f },
        { 730.f, 245.f, 3.f  },
        { 65.f,  195.f, 3.5f },
        { 395.f, 520.f, 4.f  },
        { 540.f, 510.f, 3.f  },
        { 250.f, 510.f, 3.5f },
        { 700.f, 530.f, 3.f  },
        { 110.f, 530.f, 3.f  },
        { 475.f, 245.f, 2.5f },
    };
    g.setColour (LiminalLookAndFeel::GOLD_DIM.withAlpha (0.5f));
    for (const auto& s : stars)
        draw4PointStar (g, s.x, s.y, s.r);
}

// ── Main paint ────────────────────────────────────────────────────────────────

void PluginEditor::paint (juce::Graphics& g)
{
    // ── Deep space nebula background ─────────────────────────────────────────
    g.fillAll (LiminalLookAndFeel::COBALT);

    // Large teal-blue nebula cloud (right-center)
    {
        juce::ColourGradient c (juce::Colour (0x28006888), 560.f, 200.f,
                                 juce::Colour (0x00006888), 800.f, 400.f, true);
        g.setGradientFill (c); g.fillRect (getLocalBounds());
    }
    // Secondary cloud (left-center)
    {
        juce::ColourGradient c (juce::Colour (0x1a003366), 200.f, 280.f,
                                 juce::Colour (0x00003366), 520.f, 520.f, true);
        g.setGradientFill (c); g.fillRect (getLocalBounds());
    }
    // Warm teal accent (upper-center)
    {
        juce::ColourGradient c (juce::Colour (0x22004455), 400.f, 90.f,
                                 juce::Colour (0x00004455), 680.f, 290.f, true);
        g.setGradientFill (c); g.fillRect (getLocalBounds());
    }
    // Deep purple-cobalt near bottom
    {
        juce::ColourGradient c (juce::Colour (0x1a200040), 400.f, 500.f,
                                 juce::Colour (0x00200040), 400.f, 300.f, false);
        g.setGradientFill (c); g.fillRect (getLocalBounds());
    }

    // Deterministic star field
    {
        uint32_t seed = 0xDEADBEEFu;
        auto rng = [&]() -> float {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<float> (seed & 0xffff) / 65535.f;
        };
        for (int i = 0; i < 130; ++i)
        {
            const float sx    = rng() * 800.f;
            const float sy    = rng() * 568.f;
            const float ss    = 0.4f + rng() * 1.4f;
            const float alpha = 0.15f + rng() * 0.55f;
            const int   type  = static_cast<int> (rng() * 10.f);

            if      (type < 7) g.setColour (juce::Colour (0xffe8e8f0).withAlpha (alpha * 0.55f));
            else if (type < 9) g.setColour (juce::Colour (0xffa8c4e8).withAlpha (alpha * 0.45f));
            else               g.setColour (juce::Colour (0xffc9a84c).withAlpha (alpha * 0.35f));

            g.fillEllipse (sx, sy, ss, ss);
        }
    }

    // ── Gold filigree border ─────────────────────────────────────────────────
    drawFiligreeBorder (g);

    // ── Celestial scatter decorations ────────────────────────────────────────
    drawCelestialDecorations (g);

    // ── Title: LIMINAL ───────────────────────────────────────────────────────
    g.setColour (LiminalLookAndFeel::GHOST_WHITE);
    g.setFont (juce::Font (juce::FontOptions().withHeight (18.f)).withExtraKerningFactor (0.3f));
    g.drawText ("LIMINAL", 18, 12, 200, 24, juce::Justification::left);

    // ── Manufacturer: AMENT AUDIO ────────────────────────────────────────────
    g.setColour (LiminalLookAndFeel::GOLD_DIM);
    g.setFont (juce::Font (juce::FontOptions().withHeight (9.f)));
    g.drawText ("AMENT AUDIO", getWidth() - 110, 14, 98, 14, juce::Justification::right);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    // Header strip
    area.removeFromTop (32);

    // Central display
    const int displayH = 180;
    thresholdDisplay.setBounds (area.removeFromTop (displayH).reduced (8, 0));

    area.removeFromTop (8);

    // Top row: 5 knobs + INV toggle
    const int knobRowH = 72;
    auto knobRow = area.removeFromTop (knobRowH);
    invertButton .setBounds (knobRow.removeFromRight (36).reduced (4, 20));
    const int knobW = knobRow.getWidth() / 5;
    knobThreshold.setBounds (knobRow.removeFromLeft (knobW));
    knobSlew     .setBounds (knobRow.removeFromLeft (knobW));
    knobDepth    .setBounds (knobRow.removeFromLeft (knobW));
    knobTone     .setBounds (knobRow.removeFromLeft (knobW));
    knobMix      .setBounds (knobRow);

    area.removeFromTop (8);

    // Engine panels row
    const int panelH = 110;
    auto panelRow = area.removeFromTop (panelH);
    const int panelW = panelRow.getWidth() / 3;
    hauntPanel  .setBounds (panelRow.removeFromLeft (panelW).reduced (4, 0));
    shimmerPanel.setBounds (panelRow.removeFromLeft (panelW).reduced (4, 0));
    ghostPanel  .setBounds (panelRow.reduced (4, 0));

    area.removeFromTop (8);

    // Modulation row
    const int modRowH = 60;
    auto modRow = area.removeFromTop (modRowH);
    const int modKnobW = modRow.getWidth() / 5;
    knobLfoRate          .setBounds (modRow.removeFromLeft (modKnobW));
    knobLfoToDepth       .setBounds (modRow.removeFromLeft (modKnobW));
    knobLfoToHaunt       .setBounds (modRow.removeFromLeft (modKnobW));
    knobLfoToCrystallize .setBounds (modRow.removeFromLeft (modKnobW));
    knobEnvToDepth       .setBounds (modRow);

    area.removeFromTop (8);

    // Ramp strip at bottom
    auto rampRow = area;
    knobRampTime.setBounds (rampRow.removeFromRight (60));
    latchButton .setBounds (rampRow.removeFromRight (52).reduced (0, 16));
    rampBButton .setBounds (rampRow.removeFromRight (28).reduced (2, 10));
    rampAButton .setBounds (rampRow.removeFromLeft  (28).reduced (2, 10));
    rampSlider  .setBounds (rampRow.reduced (0, 14));
}
