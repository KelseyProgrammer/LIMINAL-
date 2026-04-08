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

    setSize (800, 590);
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

    // ── SLEW → star rotation speed (log-normalize 5ms–2000ms to 0–1) ─────────
    const float slewMs   = processorRef.apvts.getRawParameterValue ("slew")->load();
    const float slewNorm = (std::log (slewMs) - std::log (5.f))
                         / (std::log (2000.f) - std::log (5.f));
    thresholdDisplay.setSlewNorm  (juce::jlimit (0.f, 1.f, slewNorm));

    // ── DEPTH → star size + glow intensity ───────────────────────────────────
    thresholdDisplay.setDepthValue (processorRef.apvts.getRawParameterValue ("depth")->load());

    // ── TONE → star/ray colour temperature ───────────────────────────────────
    thresholdDisplay.setToneValue (processorRef.apvts.getRawParameterValue ("tone")->load());

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

// Draw an astrolabe-style circular medallion: concentric rings + crosshairs +
// crescent moon ornaments on the ring + scrollwork arms.
// cx/cy is the medallion centre; r is the outer ring radius.
void PluginEditor::drawCornerMedallion (juce::Graphics& g, float cx, float cy, float r)
{
    const float alpha = 0.80f;
    const float pi    = juce::MathConstants<float>::pi;

    // ── Outer glow halo behind the ring ───────────────────────────────────────
    {
        juce::ColourGradient halo (LiminalLookAndFeel::GOLD.withAlpha (0.10f),
                                    cx, cy,
                                    juce::Colours::transparentBlack,
                                    cx + r * 1.25f, cy, true);
        g.setGradientFill (halo);
        g.fillEllipse (cx - r * 1.25f, cy - r * 1.25f, r * 2.5f, r * 2.5f);
    }

    // Outer circle (main ring)
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha));
    g.drawEllipse (cx - r, cy - r, r * 2.f, r * 2.f, 1.5f);

    // Middle ring (concentric)
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha * 0.55f));
    g.drawEllipse (cx - r * 0.62f, cy - r * 0.62f, r * 1.24f, r * 1.24f, 0.9f);

    // Inner ring
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha * 0.38f));
    g.drawEllipse (cx - r * 0.32f, cy - r * 0.32f, r * 0.64f, r * 0.64f, 0.65f);

    // Cardinal crosshair lines (extend slightly beyond outer ring)
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha * 0.52f));
    g.drawLine (cx - r * 1.15f, cy, cx + r * 1.15f, cy, 0.85f);
    g.drawLine (cx, cy - r * 1.15f, cx, cy + r * 1.15f, 0.85f);

    // Diagonal crosshairs
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha * 0.25f));
    const float d = r * 0.80f;
    g.drawLine (cx - d, cy - d, cx + d, cy + d, 0.6f);
    g.drawLine (cx + d, cy - d, cx - d, cy + d, 0.6f);

    // ── Tick marks on outer ring (16 ticks: 8 major, 8 minor) ─────────────────
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha * 0.70f));
    for (int i = 0; i < 16; ++i)
    {
        const float a  = i * pi / 8.f;
        const bool  major = (i % 2 == 0);
        const float tl = major ? 5.f : 2.5f;
        g.drawLine (cx + std::cos(a) * (r - tl), cy + std::sin(a) * (r - tl),
                    cx + std::cos(a) * r,         cy + std::sin(a) * r,
                    major ? 1.2f : 0.65f);
    }

    // ── Crescent moons sitting on the outer ring at cardinal positions ─────────
    // These are the prominent crescent ornaments visible on the medallion rings
    // in the reference image — one at top, one at left/right (corner-side).
    {
        // Draw a crescent: outer arc at r+crescR, inner arc cuts inward
        auto drawRingCrescent = [&](float angle, float crescR)
        {
            const float ax = cx + std::cos(angle) * r;
            const float ay = cy + std::sin(angle) * r;
            // Outer arc of crescent
            juce::Path c;
            c.addCentredArc (ax, ay, crescR, crescR, angle,
                              pi * 0.55f, pi * 1.45f, true);
            g.strokePath (c, juce::PathStrokeType (1.1f));
            // Inner offset arc (makes the crescent shape)
            const float ox = ax + std::cos(angle) * crescR * 0.50f;
            const float oy = ay + std::sin(angle) * crescR * 0.50f;
            juce::Path c2;
            c2.addCentredArc (ox, oy, crescR * 0.72f, crescR * 0.72f, angle,
                               pi * 0.50f, pi * 1.50f, true);
            g.strokePath (c2, juce::PathStrokeType (0.7f));
        };

        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha * 0.88f));
        // Cardinal crescent ornaments — at top, bottom, left, right of the ring
        drawRingCrescent (-pi * 0.5f, r * 0.28f);  // top
        drawRingCrescent ( pi * 0.5f, r * 0.28f);  // bottom
        drawRingCrescent (0.f,         r * 0.22f); // right
        drawRingCrescent (pi,          r * 0.22f); // left
    }

    // ── Small 4-point star at centre ──────────────────────────────────────────
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha * 0.88f));
    draw4PointStar (g, cx, cy, r * 0.20f);

    // ── Tiny dot at very centre ────────────────────────────────────────────────
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha));
    g.fillEllipse (cx - 2.f, cy - 2.f, 4.f, 4.f);
}

void PluginEditor::drawFiligreeBorder (juce::Graphics& g)
{
    const float W      = static_cast<float> (getWidth());
    const float H      = static_cast<float> (getHeight());
    const float inset  = 6.f;
    const float cornerR = 8.f;

    // ── Outer border line ─────────────────────────────────────────────────────
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.92f));
    g.drawRoundedRectangle (inset, inset,
                             W - inset * 2.f, H - inset * 2.f, cornerR, 1.8f);

    // ── Second outer border (double-line header effect) ───────────────────────
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.28f));
    g.drawRoundedRectangle (inset + 3.5f, inset + 3.5f,
                             W - (inset + 3.5f) * 2.f, H - (inset + 3.5f) * 2.f,
                             cornerR - 1.f, 0.7f);

    // ── Inner border line ─────────────────────────────────────────────────────
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.22f));
    g.drawRoundedRectangle (inset + 7.f, inset + 7.f,
                             W - (inset + 7.f) * 2.f, H - (inset + 7.f) * 2.f,
                             cornerR - 2.f, 0.6f);

    // ── Large circular medallion ornaments in each corner ─────────────────────
    const float mr = 50.f;  // medallion radius — larger to match reference
    const float mo = 60.f;  // inset from corner edge to medallion centre

    struct CornerPos { float cx, cy; };
    const CornerPos cp[4] = {
        { mo,      mo      },   // top-left
        { W - mo,  mo      },   // top-right
        { mo,      H - mo  },   // bottom-left
        { W - mo,  H - mo  }    // bottom-right
    };

    for (const auto& c : cp)
        drawCornerMedallion (g, c.cx, c.cy, mr);

    // ── Crescent moons flanking each corner medallion ────────────────────────
    // In the reference: large crescents sit to the INSIDE of each medallion
    // along the connecting lines, prominently visible.
    const float cr = 13.f;

    // Top-left: crescent to right (along top border), and below (along left border)
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.80f));
    drawCrescent (g, cp[0].cx + mr + cr * 1.2f, cp[0].cy,             cr);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.70f));
    drawCrescent (g, cp[0].cx,                   cp[0].cy + mr + cr,   cr * 0.90f);
    // Secondary smaller crescent further inward
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.45f));
    drawCrescent (g, cp[0].cx + mr + cr * 2.8f,  cp[0].cy,             cr * 0.65f);

    // Top-right: mirror
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.80f));
    drawCrescent (g, cp[1].cx - mr - cr * 1.2f, cp[1].cy,             cr);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.70f));
    drawCrescent (g, cp[1].cx,                   cp[1].cy + mr + cr,   cr * 0.90f);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.45f));
    drawCrescent (g, cp[1].cx - mr - cr * 2.8f,  cp[1].cy,             cr * 0.65f);

    // Bottom-left
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.72f));
    drawCrescent (g, cp[2].cx + mr + cr * 1.1f, cp[2].cy,             cr * 0.88f);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.60f));
    drawCrescent (g, cp[2].cx,                   cp[2].cy - mr - cr,   cr * 0.80f);

    // Bottom-right
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.72f));
    drawCrescent (g, cp[3].cx - mr - cr * 1.1f, cp[3].cy,             cr * 0.88f);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.60f));
    drawCrescent (g, cp[3].cx,                   cp[3].cy - mr - cr,   cr * 0.80f);

    // ── Scrollwork S-curves from corner medallions ────────────────────────────
    // Ornate vine arms in the reference — visible S-curves with loop tips.
    {
        const float pi = juce::MathConstants<float>::pi;

        // Single clean scroll arm: bezier S-curve from start to end with a loop tip
        auto drawScrollArm = [&](float x0, float y0, float ctrlX, float ctrlY,
                                  float x1, float y1, float loopR)
        {
            juce::Path arm;
            arm.startNewSubPath (x0, y0);
            arm.quadraticTo (ctrlX, ctrlY, x1, y1);
            g.strokePath (arm, juce::PathStrokeType (1.0f));

            // Terminal loop (small clockwise circle at tip)
            juce::Path loop;
            loop.addCentredArc (x1, y1, loopR, loopR, 0.f, 0.f, pi * 2.f, true);
            g.strokePath (loop, juce::PathStrokeType (0.85f));
        };

        // ─ Top-left corner scrollwork ─────────────────────────────────────────
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.62f));
        // Arm 1: curves right along top border
        drawScrollArm (cp[0].cx + mr * 0.85f, cp[0].cy - mr * 0.55f,
                        cp[0].cx + mr + 28.f,   inset + 8.f,
                        cp[0].cx + mr + 60.f,   inset + 16.f, 6.f);
        // Arm 2: S-curve further right
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.42f));
        drawScrollArm (cp[0].cx + mr + 58.f, inset + 14.f,
                        cp[0].cx + mr + 80.f,   inset + 24.f,
                        cp[0].cx + mr + 88.f,   inset + 36.f, 4.5f);
        // Arm 3: curves down left border
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.62f));
        drawScrollArm (cp[0].cx - mr * 0.55f, cp[0].cy + mr * 0.85f,
                        inset + 8.f, cp[0].cy + mr + 28.f,
                        inset + 16.f, cp[0].cy + mr + 60.f, 6.f);
        // Arm 4: S-curve further down
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.42f));
        drawScrollArm (inset + 14.f, cp[0].cy + mr + 58.f,
                        inset + 24.f, cp[0].cy + mr + 80.f,
                        inset + 36.f, cp[0].cy + mr + 88.f, 4.5f);
        // Inner diagonal flourish
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.35f));
        {
            juce::Path diag;
            diag.startNewSubPath (cp[0].cx + mr * 0.65f, cp[0].cy + mr * 0.65f);
            diag.cubicTo (cp[0].cx + mr * 1.0f, cp[0].cy + mr * 1.2f,
                          cp[0].cx + mr * 1.4f, cp[0].cy + mr * 0.9f,
                          cp[0].cx + mr * 1.55f, cp[0].cy + mr * 1.4f);
            g.strokePath (diag, juce::PathStrokeType (0.75f));
        }

        // ─ Top-right corner scrollwork (mirror) ──────────────────────────────
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.62f));
        drawScrollArm (cp[1].cx - mr * 0.85f, cp[1].cy - mr * 0.55f,
                        cp[1].cx - mr - 28.f,   inset + 8.f,
                        cp[1].cx - mr - 60.f,   inset + 16.f, 6.f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.42f));
        drawScrollArm (cp[1].cx - mr - 58.f, inset + 14.f,
                        cp[1].cx - mr - 80.f,   inset + 24.f,
                        cp[1].cx - mr - 88.f,   inset + 36.f, 4.5f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.62f));
        drawScrollArm (cp[1].cx + mr * 0.55f, cp[1].cy + mr * 0.85f,
                        W - inset - 8.f, cp[1].cy + mr + 28.f,
                        W - inset - 16.f, cp[1].cy + mr + 60.f, 6.f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.42f));
        drawScrollArm (W - inset - 14.f, cp[1].cy + mr + 58.f,
                        W - inset - 24.f, cp[1].cy + mr + 80.f,
                        W - inset - 36.f, cp[1].cy + mr + 88.f, 4.5f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.35f));
        {
            juce::Path diag;
            diag.startNewSubPath (cp[1].cx - mr * 0.65f, cp[1].cy + mr * 0.65f);
            diag.cubicTo (cp[1].cx - mr * 1.0f, cp[1].cy + mr * 1.2f,
                          cp[1].cx - mr * 1.4f, cp[1].cy + mr * 0.9f,
                          cp[1].cx - mr * 1.55f, cp[1].cy + mr * 1.4f);
            g.strokePath (diag, juce::PathStrokeType (0.75f));
        }

        // ─ Bottom-left corner scrollwork ─────────────────────────────────────
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.62f));
        drawScrollArm (cp[2].cx + mr * 0.85f, cp[2].cy + mr * 0.55f,
                        cp[2].cx + mr + 28.f,   H - inset - 8.f,
                        cp[2].cx + mr + 60.f,   H - inset - 16.f, 6.f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.42f));
        drawScrollArm (cp[2].cx + mr + 58.f, H - inset - 14.f,
                        cp[2].cx + mr + 80.f,   H - inset - 24.f,
                        cp[2].cx + mr + 88.f,   H - inset - 36.f, 4.5f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.62f));
        drawScrollArm (cp[2].cx - mr * 0.55f, cp[2].cy - mr * 0.85f,
                        inset + 8.f, cp[2].cy - mr - 28.f,
                        inset + 16.f, cp[2].cy - mr - 60.f, 6.f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.42f));
        drawScrollArm (inset + 14.f, cp[2].cy - mr - 58.f,
                        inset + 24.f, cp[2].cy - mr - 80.f,
                        inset + 36.f, cp[2].cy - mr - 88.f, 4.5f);

        // ─ Bottom-right corner scrollwork ────────────────────────────────────
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.62f));
        drawScrollArm (cp[3].cx - mr * 0.85f, cp[3].cy + mr * 0.55f,
                        cp[3].cx - mr - 28.f,   H - inset - 8.f,
                        cp[3].cx - mr - 60.f,   H - inset - 16.f, 6.f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.42f));
        drawScrollArm (cp[3].cx - mr - 58.f, H - inset - 14.f,
                        cp[3].cx - mr - 80.f,   H - inset - 24.f,
                        cp[3].cx - mr - 88.f,   H - inset - 36.f, 4.5f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.62f));
        drawScrollArm (cp[3].cx + mr * 0.55f, cp[3].cy - mr * 0.85f,
                        W - inset - 8.f, cp[3].cy - mr - 28.f,
                        W - inset - 16.f, cp[3].cy - mr - 60.f, 6.f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.42f));
        drawScrollArm (W - inset - 14.f, cp[3].cy - mr - 58.f,
                        W - inset - 24.f, cp[3].cy - mr - 80.f,
                        W - inset - 36.f, cp[3].cy - mr - 88.f, 4.5f);
    }

    // ── Connecting lines from medallions to border edges ──────────────────────
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.48f));
    // Top edge connections (double lines)
    g.drawLine (cp[0].cx + mr, inset,        cp[1].cx - mr, inset,        1.0f);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.22f));
    g.drawLine (cp[0].cx + mr, inset + 4.f,  cp[1].cx - mr, inset + 4.f,  0.6f);
    // Bottom edge connections
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.48f));
    g.drawLine (cp[2].cx + mr, H - inset,    cp[3].cx - mr, H - inset,    1.0f);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.22f));
    g.drawLine (cp[2].cx + mr, H-inset-4.f,  cp[3].cx - mr, H-inset-4.f,  0.6f);
    // Left edge connections
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.48f));
    g.drawLine (inset,     cp[0].cy + mr,    inset,     cp[2].cy - mr,    1.0f);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.22f));
    g.drawLine (inset+4.f, cp[0].cy + mr,    inset+4.f, cp[2].cy - mr,    0.6f);
    // Right edge connections
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.48f));
    g.drawLine (W - inset,     cp[1].cy + mr, W - inset,     cp[3].cy - mr, 1.0f);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.22f));
    g.drawLine (W - inset - 4.f, cp[1].cy + mr, W - inset - 4.f, cp[3].cy - mr, 0.6f);

    // ── Horizontal section dividers ───────────────────────────────────────────
    // Key Y positions matching resized() layout: display bottom ~259, knob bottom ~339,
    // engine bottom ~457, mod bottom ~525.
    auto drawDivider = [&](float y, float alphaMain, float ornamentR)
    {
        const float xL = cp[0].cx + mr + 4.f;  // left of medallion area
        const float xR = cp[1].cx - mr - 4.f;  // right of medallion area
        const float xC = W * 0.5f;

        // Main horizontal line
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alphaMain));
        g.drawLine (xL, y, xC - ornamentR - 8.f, y, 0.8f);
        g.drawLine (xC + ornamentR + 8.f, y, xR, y, 0.8f);

        // Parallel secondary line
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alphaMain * 0.35f));
        g.drawLine (xL + 8.f, y + 4.f, xC - ornamentR - 4.f, y + 4.f, 0.5f);
        g.drawLine (xC + ornamentR + 4.f, y + 4.f, xR - 8.f, y + 4.f, 0.5f);

        // Center diamond ornament
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alphaMain * 1.1f));
        draw4PointStar (g, xC, y + 2.f, ornamentR);

        // Small flanking dots
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alphaMain * 0.60f));
        g.fillEllipse (xC - ornamentR - 14.f, y, 3.f, 3.f);
        g.fillEllipse (xC + ornamentR + 11.f, y, 3.f, 3.f);
    };

    // Below display / above knob row
    drawDivider (261.f, 0.52f, 6.5f);
    // Below knob row / above engine panels
    drawDivider (343.f, 0.40f, 5.f);
    // Below engine panels / above mod row
    drawDivider (460.f, 0.38f, 4.5f);
    // Below mod row / above ramp
    drawDivider (528.f, 0.30f, 4.f);
    // Below header / above display (under title)
    drawDivider (42.f, 0.42f, 5.5f);

    // ── Top center ornament ───────────────────────────────────────────────────
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.88f));
    draw4PointStar (g, W * 0.5f, inset, 7.5f);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.38f));
    draw4PointStar (g, W * 0.5f - 28.f, inset, 3.f);
    draw4PointStar (g, W * 0.5f + 28.f, inset, 3.f);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.25f));
    draw4PointStar (g, W * 0.5f - 48.f, inset, 2.f);
    draw4PointStar (g, W * 0.5f + 48.f, inset, 2.f);

    // ── Bottom center ornament ────────────────────────────────────────────────
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.85f));
    draw4PointStar (g, W * 0.5f, H - inset, 7.5f);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.38f));
    draw4PointStar (g, W * 0.5f - 28.f, H - inset, 3.f);
    draw4PointStar (g, W * 0.5f + 28.f, H - inset, 3.f);

    // ── Mid-side diamonds (left and right borders) ────────────────────────────
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.58f));
    const float dSize = 5.f;
    for (float dx : { inset, W - inset })
    {
        juce::Path d;
        d.startNewSubPath (dx,         H * 0.5f - dSize);
        d.lineTo          (dx + dSize, H * 0.5f);
        d.lineTo          (dx,         H * 0.5f + dSize);
        d.lineTo          (dx - dSize, H * 0.5f);
        d.closeSubPath();
        g.strokePath (d, juce::PathStrokeType (1.0f));
    }
    // Secondary side ornaments at quarter-height positions
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.32f));
    for (float dx : { inset, W - inset })
    {
        g.fillEllipse (dx - 1.5f, H * 0.33f - 1.5f, 3.f, 3.f);
        g.fillEllipse (dx - 1.5f, H * 0.66f - 1.5f, 3.f, 3.f);
    }

    // ── Small crescent moons along the side borders ───────────────────────────
    g.setColour (LiminalLookAndFeel::GOLD_DIM.withAlpha (0.42f));
    drawCrescent (g, inset,     H * 0.38f,  6.f);
    drawCrescent (g, inset,     H * 0.62f,  6.f);
    drawCrescent (g, W - inset, H * 0.38f,  6.f);
    drawCrescent (g, W - inset, H * 0.62f,  6.f);
}

void PluginEditor::drawCelestialDecorations (juce::Graphics& g)
{
    const float W = static_cast<float> (getWidth());

    // ── Crescent moons — positioned to match the reference image ─────────────
    struct Moon { float x, y, r, alpha; };
    const Moon moons[] = {
        // Flanking star display (prominently placed left and right of central display)
        { 75.f,  130.f, 11.f, 0.68f },
        { 725.f, 130.f, 11.f, 0.68f },
        { 78.f,  200.f,  8.f, 0.52f },
        { 722.f, 200.f,  8.f, 0.52f },
        // Small flanking moons near star bottom
        { 150.f, 245.f,  6.f, 0.40f },
        { 650.f, 245.f,  6.f, 0.40f },
        // Knob row area (between/around the 5 main knobs)
        { 55.f,  305.f,  8.f, 0.52f },
        { 745.f, 305.f,  8.f, 0.52f },
        { 205.f, 278.f,  5.f, 0.38f },
        { 355.f, 278.f,  5.f, 0.35f },
        { 450.f, 278.f,  5.f, 0.35f },
        { 600.f, 278.f,  5.f, 0.38f },
        // Engine panel row flanks
        { 60.f,  400.f,  7.f, 0.45f },
        { 740.f, 400.f,  7.f, 0.45f },
        // Below engine panels / mod row
        { 88.f,  475.f,  5.f, 0.38f },
        { 712.f, 475.f,  5.f, 0.38f },
        { 240.f, 487.f,  4.f, 0.30f },
        { 560.f, 487.f,  4.f, 0.30f },
        // Ramp/bottom area
        { 120.f, 550.f,  5.f, 0.32f },
        { 680.f, 550.f,  5.f, 0.32f },
    };
    for (const auto& m : moons)
    {
        g.setColour (LiminalLookAndFeel::GOLD_DIM.withAlpha (m.alpha));
        drawCrescent (g, m.x, m.y, m.r);
    }

    // ── Between-knob diamond separators in the main knob row ──────────────────
    // Small diamond ornaments sit between the 5 main knobs (4 gaps).
    // Knob row is Y≈267–339, knobs are evenly spaced across ~720px (12–792 reduced).
    // Each knob occupies ~144px. Gap centers at: ~156, ~300, ~444, ~588 (from x=84).
    {
        const float knobRowY = 303.f;  // vertical centre of knob row
        const float knobW    = (W - 24.f - 36.f) / 5.f;   // approx knob width (minus inv btn)
        const float knobX0   = 12.f;

        const float gapPositions[] = {
            knobX0 + knobW * 1.0f,
            knobX0 + knobW * 2.0f,
            knobX0 + knobW * 3.0f,
            knobX0 + knobW * 4.0f,
        };

        g.setColour (LiminalLookAndFeel::GOLD_DIM.withAlpha (0.38f));
        for (float gx : gapPositions)
        {
            const float ds = 3.5f;
            juce::Path d;
            d.startNewSubPath (gx,      knobRowY - ds);
            d.lineTo          (gx + ds, knobRowY);
            d.lineTo          (gx,      knobRowY + ds);
            d.lineTo          (gx - ds, knobRowY);
            d.closeSubPath();
            g.strokePath (d, juce::PathStrokeType (0.7f));

            // Small dots above/below each diamond
            g.setColour (LiminalLookAndFeel::GOLD_DIM.withAlpha (0.25f));
            g.fillEllipse (gx - 1.f, knobRowY - ds - 5.f, 2.f, 2.f);
            g.fillEllipse (gx - 1.f, knobRowY + ds + 3.f, 2.f, 2.f);
            g.setColour (LiminalLookAndFeel::GOLD_DIM.withAlpha (0.38f));
        }
    }

    // ── Engine panel flanking ornaments ───────────────────────────────────────
    // Small star + crescent pairs flanking each engine panel header.
    // Panel centres approximate: haunt ~155, shimmer ~400, ghost ~645 (Y top ~355).
    struct EngineDeco { float x, y; };
    const EngineDeco engineDecos[] = {
        // Flanking HAUNT VERB label
        { 90.f, 362.f }, { 218.f, 362.f },
        // Flanking SHIMMER label
        { 335.f, 362.f }, { 465.f, 362.f },
        // Flanking PITCH GHOST label
        { 580.f, 362.f }, { 715.f, 362.f },
    };
    for (int i = 0; i < 6; ++i)
    {
        const auto& d = engineDecos[i];
        g.setColour (LiminalLookAndFeel::GOLD_DIM.withAlpha (0.38f));
        if (i % 2 == 0)
            drawCrescent (g, d.x, d.y, 4.5f);   // left of label: crescent
        else
            draw4PointStar (g, d.x, d.y, 3.5f); // right of label: star
    }

    // ── Sun starburst between display and knob row ────────────────────────────
    // A small ornate sun symbol sits just below the central star display,
    // above the knob row. In the reference this manifests as a concentrated
    // warm glow point. We add a small 8-ray starburst here.
    {
        const float sx = W * 0.5f;
        const float sy = 269.f;   // just below display divider line
        const float sr = 8.f;
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.55f));
        // 8-ray starburst (two overlapping 4-point stars at 45° offset)
        draw4PointStar (g, sx, sy, sr);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.35f));
        // Rotated 45° version
        juce::Path star45;
        const float pi4 = juce::MathConstants<float>::pi / 4.f;
        for (int i = 0; i < 4; ++i)
        {
            const float oa = pi4 + i * juce::MathConstants<float>::halfPi - juce::MathConstants<float>::halfPi;
            const float ia = oa + juce::MathConstants<float>::pi / 4.f;
            const juce::Point<float> op { sx + sr * std::cos(oa), sy + sr * std::sin(oa) };
            const juce::Point<float> ip { sx + sr * 0.28f * std::cos(ia), sy + sr * 0.28f * std::sin(ia) };
            if (i == 0) star45.startNewSubPath (op); else star45.lineTo (op);
            star45.lineTo (ip);
        }
        star45.closeSubPath();
        g.strokePath (star45, juce::PathStrokeType (0.8f));

        // Warm glow behind the sun ornament
        juce::ColourGradient sunGlow (juce::Colour (0xffaa44).withAlpha (0.30f),
                                       sx, sy, juce::Colour (0x00ffaa44),
                                       sx + 22.f, sy, true);
        g.setGradientFill (sunGlow);
        g.fillEllipse (sx - 22.f, sy - 22.f, 44.f, 44.f);
    }

    // ── Small 4-point stars scattered as background sparkle ───────────────────
    struct Star { float x, y, r, alpha; };
    const Star stars[] = {
        { 185.f, 195.f, 4.5f, 0.58f },
        { 620.f, 182.f, 4.0f, 0.58f },
        { 738.f, 252.f, 3.0f, 0.48f },
        { 62.f,  188.f, 3.5f, 0.48f },
        { 400.f, 542.f, 4.0f, 0.45f },
        { 545.f, 532.f, 3.0f, 0.40f },
        { 252.f, 532.f, 3.5f, 0.40f },
        { 705.f, 547.f, 3.0f, 0.40f },
        { 112.f, 547.f, 3.0f, 0.40f },
        { 472.f, 238.f, 2.5f, 0.38f },
        { 328.f, 172.f, 3.0f, 0.42f },
        { 492.f, 172.f, 3.0f, 0.42f },
        // Stars flanking the engine panel area
        { 280.f, 405.f, 2.5f, 0.32f },
        { 520.f, 405.f, 2.5f, 0.32f },
        { 165.f, 430.f, 2.0f, 0.28f },
        { 635.f, 430.f, 2.0f, 0.28f },
        // Small sparkles in knob row gaps
        { 205.f, 320.f, 2.0f, 0.30f },
        { 350.f, 316.f, 2.0f, 0.28f },
        { 455.f, 316.f, 2.0f, 0.28f },
        { 600.f, 320.f, 2.0f, 0.30f },
    };
    for (const auto& s : stars)
    {
        g.setColour (LiminalLookAndFeel::GOLD_DIM.withAlpha (s.alpha));
        draw4PointStar (g, s.x, s.y, s.r);
    }
}

// ── Main paint ────────────────────────────────────────────────────────────────

void PluginEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();

    // ── Deep space base ───────────────────────────────────────────────────────
    g.fillAll (LiminalLookAndFeel::COBALT);

    // ── MAIN NEBULA: bright teal-cyan cloud, center-upper-left ───────────────
    // This is the dominant visual element of the reference background —
    // a vivid, almost luminous teal-cyan cloud that lights the whole upper area.
    {
        // Outer haze — large, blue-teal
        juce::ColourGradient c (juce::Colour (0x9500aabb), 310.f, 80.f,
                                 juce::Colour (0x000088aa), 590.f, 310.f, true);
        g.setGradientFill (c); g.fillRect (bounds);
    }
    {
        // Bright inner core — white-teal, very luminous at the brightest point
        juce::ColourGradient c (juce::Colour (0x7055ccee), 240.f, 50.f,
                                 juce::Colour (0x000099cc), 460.f, 220.f, true);
        g.setGradientFill (c); g.fillRect (bounds);
    }
    {
        // Hottest white-blue peak
        juce::ColourGradient c (juce::Colour (0x5088ddff), 200.f, 35.f,
                                 juce::Colour (0x0077ccdd), 380.f, 180.f, true);
        g.setGradientFill (c); g.fillRect (bounds);
    }
    // ── SECONDARY NEBULA: upper-right bloom ───────────────────────────────────
    {
        juce::ColourGradient c (juce::Colour (0x7000aacc), 600.f, 55.f,
                                 juce::Colour (0x000077aa), 800.f, 240.f, true);
        g.setGradientFill (c); g.fillRect (bounds);
    }
    {
        // Smaller bright kernel in secondary cloud
        juce::ColourGradient c (juce::Colour (0x4844bbcc), 640.f, 40.f,
                                 juce::Colour (0x000088bb), 760.f, 150.f, true);
        g.setGradientFill (c); g.fillRect (bounds);
    }
    // ── LOWER-LEFT nebula extension ───────────────────────────────────────────
    {
        juce::ColourGradient c (juce::Colour (0x3400558a), 100.f, 390.f,
                                 juce::Colour (0x00002255), 320.f, 540.f, true);
        g.setGradientFill (c); g.fillRect (bounds);
    }
    // ── Mid-right faint blue-purple cloud ─────────────────────────────────────
    {
        juce::ColourGradient c (juce::Colour (0x28002a6a), 680.f, 300.f,
                                 juce::Colour (0x00001844), 820.f, 480.f, true);
        g.setGradientFill (c); g.fillRect (bounds);
    }
    // ── Edge vignette — darken toward all four sides ──────────────────────────
    {
        juce::ColourGradient v (juce::Colour (0x00000010), 400.f, 295.f,
                                 juce::Colour (0x55000020), 0.f, 0.f, true);
        g.setGradientFill (v); g.fillRect (bounds);
    }

    // ── Scattered warm amber "star glow" spots ────────────────────────────────
    // The reference has ~6-8 small warm radial glows scattered across the field,
    // like distant suns peeking through the nebula.
    {
        struct GlowSpot { float x, y, r; float alpha; };
        const GlowSpot glows[] = {
            { 182.f, 325.f, 42.f, 0.18f },   // left-center warm glow
            { 112.f, 178.f, 28.f, 0.14f },   // upper-left quadrant
            { 638.f, 442.f, 38.f, 0.16f },   // right-center warm glow
            { 698.f, 178.f, 30.f, 0.15f },   // upper-right warm point
            { 375.f, 505.f, 36.f, 0.13f },   // lower-center
            { 710.f, 355.f, 26.f, 0.13f },   // mid-right
            { 88.f,  508.f, 22.f, 0.11f },   // lower-left
            { 580.f, 295.f, 20.f, 0.12f },   // center-right area
        };
        for (const auto& gs : glows)
        {
            juce::ColourGradient glow (juce::Colour (0xffbb44).withAlpha (gs.alpha),
                                        gs.x, gs.y,
                                        juce::Colour (0x00ff9900),
                                        gs.x + gs.r, gs.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (gs.x - gs.r, gs.y - gs.r, gs.r * 2.f, gs.r * 2.f);
        }
    }

    // ── Subtle warm glow below the star display into the knob row ────────────
    // The ThresholdDisplay handles its own internal burst; this is just a
    // faint residual warm tint in the knob area so the amber light "spills down".
    {
        juce::ColourGradient spill (juce::Colour (0xff6600).withAlpha (0.10f),
                                    400.f, 262.f,
                                    juce::Colour (0x00ff6600),
                                    400.f, 380.f, false);
        g.setGradientFill (spill);
        g.fillEllipse (160.f, 258.f, 480.f, 120.f);
    }

    // ── Star field — more varied, some with cross-spike shapes ────────────────
    {
        uint32_t seed = 0xDEADBEEFu;
        auto rng = [&]() -> float {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<float> (seed & 0xffff) / 65535.f;
        };
        for (int i = 0; i < 200; ++i)
        {
            const float sx    = rng() * 800.f;
            const float sy    = rng() * 590.f;
            const float ss    = 0.5f + rng() * 2.2f;
            const float alpha = 0.28f + rng() * 0.72f;
            const int   type  = static_cast<int> (rng() * 12.f);

            if      (type < 7) g.setColour (juce::Colour (0xffe8e8f0).withAlpha (alpha * 0.70f));
            else if (type < 10) g.setColour (juce::Colour (0xffa8c4e8).withAlpha (alpha * 0.60f));
            else               g.setColour (juce::Colour (0xffc9a84c).withAlpha (alpha * 0.50f));

            if (ss > 1.8f && type > 8)
            {
                // Larger stars get a tiny 4-spike cross glow
                const juce::Colour starCol =
                    (type < 7) ? juce::Colour (0xffe8e8f0).withAlpha (alpha * 0.70f)
                  : (type < 10) ? juce::Colour (0xffa8c4e8).withAlpha (alpha * 0.60f)
                  :               juce::Colour (0xffc9a84c).withAlpha (alpha * 0.50f);
                g.setColour (starCol);
                g.fillEllipse (sx - ss * 0.5f, sy - ss * 0.5f, ss, ss);
                g.setColour (starCol.withAlpha (alpha * 0.28f));
                g.drawLine (sx, sy - ss * 2.f, sx, sy + ss * 2.f, 0.5f);
                g.drawLine (sx - ss * 2.f, sy, sx + ss * 2.f, sy, 0.5f);
            }
            else
            {
                g.fillEllipse (sx, sy, ss, ss);
            }
        }
    }

    // NOTE: border, decorations and title are drawn in paintOverChildren()
    //       so they appear on top of the ThresholdDisplay child component.
}

void PluginEditor::paintOverChildren (juce::Graphics& g)
{
    // ── Gold filigree border + medallions — drawn OVER child components ───────
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

    // Central display — taller for a more dramatic star presentation
    const int displayH = 215;
    thresholdDisplay.setBounds (area.removeFromTop (displayH).reduced (8, 0));

    area.removeFromTop (8);

    // Top row: 5 knobs + INV toggle
    const int knobRowH = 72;
    auto knobRow = area.removeFromTop (knobRowH);
    // INV button as overlay — doesn't steal width from the 5-knob division
    invertButton.setBounds (knobRow.getRight() - 36, knobRow.getY() + 20, 32, knobRow.getHeight() - 40);
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
    latchButton .setBounds (rampRow.removeFromRight (52).reduced (0, 14));
    rampBButton .setBounds (rampRow.removeFromRight (28).reduced (2, 8));
    rampAButton .setBounds (rampRow.removeFromLeft  (28).reduced (2, 8));
    rampSlider  .setBounds (rampRow.reduced (0, 12));
}
