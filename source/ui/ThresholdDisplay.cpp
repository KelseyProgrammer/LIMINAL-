#include "ThresholdDisplay.h"
#include "LiminalLookAndFeel.h"

#include <cmath>

ThresholdDisplay::ThresholdDisplay()
{
    setOpaque (false);
    startTimerHz (60);
}

ThresholdDisplay::~ThresholdDisplay()
{
    stopTimer();
}

void ThresholdDisplay::timerCallback()
{
    const float newBlend  = blendLevel.load();
    const float newEnv    = envelopeLevel.load();
    const float newThresh = thresholdVal.load();

    const bool wasAbove = prevEnvelope >= displayThreshold;
    const bool nowBelow = newEnv < newThresh;
    if (wasAbove && nowBelow && flashIntensity < 0.1f)
    {
        flashIntensity = 1.f;
        rippleRadius   = 0.f;
        rippleActive   = true;
    }
    prevEnvelope = newEnv;

    flashIntensity = std::max (0.f, flashIntensity - 0.055f);

    if (rippleActive)
    {
        rippleRadius += 2.5f;
        if (rippleRadius > 110.f)
            rippleActive = false;
    }

    // ── SLEW → rotation speed ─────────────────────────────────────────────────
    // slewNorm 0 = fastest (5ms)  → fast spin  (0.35°/frame ≈ 21°/sec at 60fps)
    // slewNorm 1 = slowest (2000ms) → barely moves (0.012°/frame ≈ 0.7°/sec)
    displaySlew  = slewNorm.load();
    displayDepth = depthVal.load();
    displayTone  = toneVal.load();

    rotationSpeed = 0.35f - displaySlew * (0.35f - 0.012f);
    starRotation += rotationSpeed;
    if (starRotation >= 360.f) starRotation -= 360.f;

    displayBlend     = newBlend;
    displayEnvelope  = newEnv;
    displayThreshold = newThresh;

    repaint();
}

void ThresholdDisplay::resized() {}

// Draw a hexagram (Star of David) — two overlapping equilateral triangles.
// outerR = tip-to-center radius, innerR = inner hexagon radius ≈ outerR * 0.38.
static void drawHexagram (juce::Graphics& g, float cx, float cy,
                           float outerR, float innerR, float rotRad,
                           juce::Colour stroke, float strokeW,
                           juce::Colour fillCol = juce::Colours::transparentBlack)
{
    const float step   = juce::MathConstants<float>::twoPi / 6.f;
    juce::Path star;
    for (int i = 0; i < 6; ++i)
    {
        const float outerA = rotRad + i * step - juce::MathConstants<float>::halfPi;
        const float innerA = outerA + step * 0.5f;
        const juce::Point<float> op { cx + outerR * std::cos (outerA),
                                      cy + outerR * std::sin (outerA) };
        const juce::Point<float> ip { cx + innerR * std::cos (innerA),
                                      cy + innerR * std::sin (innerA) };
        if (i == 0) star.startNewSubPath (op);
        else        star.lineTo (op);
        star.lineTo (ip);
    }
    star.closeSubPath();

    if (fillCol.getAlpha() > 0)
    {
        g.setColour (fillCol);
        g.fillPath (star);
    }
    g.setColour (stroke);
    g.strokePath (star, juce::PathStrokeType (strokeW,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

// Draw N radiating rays from each of the 6 hexagram tip positions.
static void drawHexRays (juce::Graphics& g, float cx, float cy,
                          float fromR, float toR, float rotRad,
                          juce::Colour col, float strokeW)
{
    g.setColour (col);
    const float step = juce::MathConstants<float>::twoPi / 6.f;
    for (int i = 0; i < 6; ++i)
    {
        const float angle = rotRad + i * step - juce::MathConstants<float>::halfPi;
        const float x0 = cx + fromR * std::cos (angle);
        const float y0 = cy + fromR * std::sin (angle);
        const float x1 = cx + toR   * std::cos (angle);
        const float y1 = cy + toR   * std::sin (angle);
        g.drawLine (x0, y0, x1, y1, strokeW);
    }
}

void ThresholdDisplay::paint (juce::Graphics& g)
{
    const auto  bounds  = getLocalBounds().toFloat();
    const auto  centre  = bounds.getCentre();
    const float minDim  = std::min (bounds.getWidth(), bounds.getHeight());
    const float blend   = displayBlend;
    const float rotRad  = starRotation * juce::MathConstants<float>::pi / 180.f;

    // ── Knob-driven display parameters ────────────────────────────────────────
    // DEPTH (0–1): scales star size and overall glow brightness.
    const float depthScale = 0.60f + displayDepth * 0.40f;

    // TONE (−1…+1): interpolates star/ray colour between cool and warm.
    // −1 = ICE_BLUE, 0 = GOLD, +1 = warm amber-white (#fce8a0)
    const float toneT = (displayTone + 1.f) * 0.5f;   // 0→1
    const juce::Colour toneColour =
        (toneT < 0.5f)
        ? LiminalLookAndFeel::ICE_BLUE.interpolatedWith (LiminalLookAndFeel::GOLD, toneT * 2.f)
        : LiminalLookAndFeel::GOLD.interpolatedWith     (juce::Colour (0xfffce8a0), (toneT - 0.5f) * 2.f);

    // Glow colour also shifts: cool blue at negative tone, warm amber at positive
    const juce::Colour glowColour =
        (toneT < 0.5f)
        ? juce::Colour (0xff3355bb).interpolatedWith (juce::Colour (0xff884400), toneT * 2.f)
        : juce::Colour (0xff884400).interpolatedWith (juce::Colour (0xffcc7700), (toneT - 0.5f) * 2.f);

    // ── Radial darkening (centres the display, lets nebula show at edges) ─────
    {
        juce::ColourGradient v (juce::Colour (0x44050a1a), centre.x, centre.y,
                                juce::Colour (0x00050a1a),
                                centre.x + minDim * 0.55f, centre.y, true);
        g.setGradientFill (v); g.fillRect (bounds);
    }

    // ── LARGE ambient glow — tinted by TONE, sized by DEPTH ──────────────────
    // Three radial layers: outer haze, mid bloom, inner core.
    // glowColour shifts from cool-blue (tone=-1) through amber (tone=0) to warm-white (tone=+1).
    {
        const float gd = depthScale;  // depth modulates brightness

        const float r1 = minDim * (0.48f + blend * 0.22f);
        juce::ColourGradient ga (glowColour.withAlpha ((0.08f + blend * 0.09f) * gd),
                                  centre.x, centre.y,
                                  glowColour.withAlpha (0.f), centre.x + r1, centre.y, true);
        g.setGradientFill (ga); g.fillEllipse (centre.x-r1, centre.y-r1, r1*2, r1*2);

        const float r2 = minDim * (0.28f + blend * 0.14f);
        juce::ColourGradient gb (glowColour.withAlpha ((0.14f + blend * 0.22f) * gd),
                                  centre.x, centre.y,
                                  glowColour.withAlpha (0.f), centre.x + r2, centre.y, true);
        g.setGradientFill (gb); g.fillEllipse (centre.x-r2, centre.y-r2, r2*2, r2*2);

        const float r3 = minDim * (0.13f + blend * 0.08f);
        juce::ColourGradient gc (glowColour.withAlpha ((0.28f + blend * 0.38f) * gd),
                                  centre.x, centre.y,
                                  glowColour.withAlpha (0.f), centre.x + r3, centre.y, true);
        g.setGradientFill (gc); g.fillEllipse (centre.x-r3, centre.y-r3, r3*2, r3*2);
    }

    // ── Downward warm burst — amber cone below the star ──────────────────────
    // In the reference this is a warm, atmospheric glow — present but not
    // dominating. The reference shows subtle amber light, not a blazing dome.
    {
        const float burstW = minDim * (0.65f + blend * 0.20f);
        const float burstH = bounds.getHeight() * (0.75f + blend * 0.15f);
        const float alpha  = 0.15f + blend * 0.22f;   // calm: visible but not overpowering

        // Wide gentle cone
        juce::ColourGradient burst (juce::Colour (0xff7700).withAlpha (alpha * 0.65f),
                                    centre.x, centre.y,
                                    juce::Colour (0x00ff5500),
                                    centre.x, centre.y + burstH, false);
        burst.addColour (0.4, juce::Colour (0xff5500).withAlpha (alpha * 0.28f));
        g.setGradientFill (burst);
        g.fillEllipse (centre.x - burstW * 0.5f, centre.y - 8.f, burstW, burstH);

        // Narrower bright core
        const float coreW = minDim * (0.22f + blend * 0.08f);
        juce::ColourGradient core (juce::Colour (0xffaa44).withAlpha (alpha * 0.85f),
                                    centre.x, centre.y,
                                    juce::Colour (0x00ffaa44),
                                    centre.x, centre.y + burstH * 0.55f, false);
        g.setGradientFill (core);
        g.fillEllipse (centre.x - coreW * 0.5f, centre.y, coreW, burstH * 0.55f);
    }

    // ── Threshold crossing flash ───────────────────────────────────────────────
    if (flashIntensity > 0.f)
    {
        const float flashR = minDim * 0.55f;
        juce::ColourGradient flash (juce::Colour (0xffffee88).withAlpha (flashIntensity * 0.35f),
                                    centre.x, centre.y,
                                    juce::Colour (0x00ffee88), centre.x + flashR, centre.y, true);
        g.setGradientFill (flash); g.fillRect (bounds);
    }

    // ── Star geometry — DEPTH scales size; calibrated to match reference ───────
    const float outerR = minDim * (0.210f + blend * 0.14f) * depthScale;
    const float innerR = outerR * 0.375f;
    const float coreR  = outerR * 0.26f;
    const float rayEnd = outerR * 2.10f;

    // ── Outer decorative ring — largest circle, very faint ────────────────────
    const float outerDecR = outerR * 2.05f;
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.18f + blend * 0.12f));
    g.drawEllipse (centre.x - outerDecR, centre.y - outerDecR,
                   outerDecR * 2.f, outerDecR * 2.f, 0.6f);

    // ── Threshold ring (gold, maps to threshold param) ────────────────────────
    // Sits between outer decorative ring and compass ring.
    const float threshR = outerR * (1.60f + displayThreshold * 0.35f);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.55f));
    g.drawEllipse (centre.x - threshR, centre.y - threshR,
                   threshR * 2.f, threshR * 2.f, 1.8f);
    // Second parallel threshold line (double-ring effect)
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.22f));
    g.drawEllipse (centre.x - threshR - 4.f, centre.y - threshR - 4.f,
                   (threshR + 4.f) * 2.f, (threshR + 4.f) * 2.f, 0.7f);

    // ── Envelope ring (ice blue, moves with signal) ────────────────────────────
    const float envR = outerR * (1.60f + displayEnvelope * 0.35f);
    g.setColour (LiminalLookAndFeel::ICE_BLUE.withAlpha (0.42f));
    g.drawEllipse (centre.x - envR, centre.y - envR,
                   envR * 2.f, envR * 2.f, 1.2f);

    // ── Main compass ring — the prominent ornate circle around the star ────────
    const float compassR = outerR * 1.70f;

    // Glow halo behind the compass ring
    {
        juce::ColourGradient halo (LiminalLookAndFeel::GOLD.withAlpha (0.10f + blend * 0.08f),
                                    centre.x, centre.y,
                                    juce::Colours::transparentBlack,
                                    centre.x + compassR * 1.1f, centre.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (centre.x - compassR * 1.1f, centre.y - compassR * 1.1f,
                       compassR * 2.2f, compassR * 2.2f);
    }

    // Main ring — thick and bright
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.72f + blend * 0.22f));
    g.drawEllipse (centre.x - compassR, centre.y - compassR,
                   compassR * 2.f, compassR * 2.f, 1.8f);

    // Second parallel ring just inside (double-line like a navigational compass)
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.28f + blend * 0.15f));
    g.drawEllipse (centre.x - compassR * 0.94f, centre.y - compassR * 0.94f,
                   compassR * 1.88f, compassR * 1.88f, 0.7f);

    // ── 24 tick marks on compass ring (8 major, 8 medium, 8 minor) ────────────
    for (int i = 0; i < 24; ++i)
    {
        const float a = i * juce::MathConstants<float>::twoPi / 24.f;
        const bool  major  = (i % 6 == 0);   // 4 cardinal major ticks
        const bool  medium = (i % 3 == 0);   // 8 medium ticks
        const float inset  = major ? 9.f : (medium ? 6.f : 3.5f);
        const float weight = major ? 1.8f : (medium ? 1.1f : 0.6f);
        const float alpha  = major ? 0.92f : (medium ? 0.68f : 0.42f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha));
        g.drawLine (centre.x + std::cos(a) * (compassR - inset),
                    centre.y + std::sin(a) * (compassR - inset),
                    centre.x + std::cos(a) * compassR,
                    centre.y + std::sin(a) * compassR,
                    weight);
    }

    // ── Inner ring (between compass and star, like an astrolabe band) ──────────
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.30f + blend * 0.20f));
    g.drawEllipse (centre.x - outerR * 1.20f, centre.y - outerR * 1.20f,
                   outerR * 2.40f, outerR * 2.40f, 0.75f);

    // ── Rays from the 6 hexagram tips — TONE sets the colour ──────────────────
    // Glow layer (thick, transparent)
    drawHexRays (g, centre.x, centre.y, outerR * 0.95f, rayEnd, rotRad,
                 toneColour.withAlpha ((0.18f + blend * 0.22f) * 0.45f), 4.5f);
    // Core rays (thin, bright)
    drawHexRays (g, centre.x, centre.y, outerR * 0.95f, rayEnd, rotRad,
                 toneColour.withAlpha (0.30f + blend * 0.50f), 1.0f);

    // ── Main hexagram star — TONE colours the outline ─────────────────────────
    {
        const float alpha = 0.40f + blend * 0.55f;

        // Fill: blend between cool translucent blue and warm amber based on tone
        const juce::Colour fillCol =
            (toneT < 0.5f)
            ? juce::Colour (0x220044cc).interpolatedWith (juce::Colour (0xffcc7700).withAlpha (alpha * 0.22f), toneT * 2.f)
            : juce::Colour (0xffcc7700).withAlpha (alpha * 0.22f);
        drawHexagram (g, centre.x, centre.y, outerR, innerR, rotRad,
                      juce::Colours::transparentBlack, 0, fillCol);

        // Bloom glow tinted by tone (softer than before — keeps gold visible)
        drawHexagram (g, centre.x, centre.y, outerR, innerR, rotRad,
                      toneColour.withAlpha (alpha * 0.20f), 4.5f);

        // Core line: tone colour, warm — NOT brighter() to preserve the gold hue
        drawHexagram (g, centre.x, centre.y, outerR, innerR, rotRad,
                      toneColour.withAlpha (alpha * 0.90f), 1.4f);
    }

    // ── Inner 4-pointed star (counter-rotating bright diamond at center) ──────
    {
        const float innerAlpha = 0.60f + blend * 0.40f;
        const float negRot = -rotRad * 0.6f;
        const float step4  = juce::MathConstants<float>::twoPi / 4.f;
        juce::Path star4;
        for (int i = 0; i < 4; ++i)
        {
            const float oa = negRot + i * step4 - juce::MathConstants<float>::halfPi;
            const float ia = oa + step4 * 0.5f;
            const juce::Point<float> op { centre.x + coreR * std::cos (oa),
                                          centre.y + coreR * std::sin (oa) };
            const juce::Point<float> ip { centre.x + coreR * 0.28f * std::cos (ia),
                                          centre.y + coreR * 0.28f * std::sin (ia) };
            if (i == 0) star4.startNewSubPath (op); else star4.lineTo (op);
            star4.lineTo (ip);
        }
        star4.closeSubPath();

        // Glow
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (innerAlpha * 0.35f));
        g.strokePath (star4, juce::PathStrokeType (4.5f));
        // Core
        g.setColour (juce::Colour (0xfffce8a0).withAlpha (innerAlpha));
        g.strokePath (star4, juce::PathStrokeType (1.2f));
    }

    // ── Center bright point ────────────────────────────────────────────────────
    {
        const float dotR = coreR * 0.20f;
        juce::ColourGradient cg (juce::Colour (0xfffce8a0).withAlpha (0.92f),
                                  centre.x, centre.y,
                                  juce::Colour (0x00ffe880),
                                  centre.x + coreR * 0.5f, centre.y, true);
        g.setGradientFill (cg);
        g.fillEllipse (centre.x - coreR * 0.5f, centre.y - coreR * 0.5f, coreR, coreR);
        g.setColour (juce::Colours::white.withAlpha (0.97f));
        g.fillEllipse (centre.x - dotR, centre.y - dotR, dotR * 2.f, dotR * 2.f);
    }

    // ── Per-engine axis pulses ─────────────────────────────────────────────────
    drawEngineAxes (g, centre, outerR, blend, rotRad);

    // ── Ripple ring on threshold crossing ─────────────────────────────────────
    if (rippleActive && rippleRadius > 0.f)
    {
        const float alpha = 0.70f * (1.f - rippleRadius / 110.f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha));
        g.drawEllipse (centre.x - rippleRadius, centre.y - rippleRadius,
                       rippleRadius * 2.f, rippleRadius * 2.f, 1.6f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha * 0.35f));
        g.drawEllipse (centre.x - rippleRadius - 4.f, centre.y - rippleRadius - 4.f,
                       (rippleRadius + 4.f) * 2.f, (rippleRadius + 4.f) * 2.f, 0.7f);
    }
}

void ThresholdDisplay::drawEngineAxes (juce::Graphics& g,
                                        juce::Point<float> centre,
                                        float outerR, float blend,
                                        float rotRad)
{
    const bool engines[3] = {
        hauntActive.load(), shimmerActive.load(), ghostActive.load()
    };
    const juce::Colour engineColors[3] = {
        LiminalLookAndFeel::ICE_BLUE,
        LiminalLookAndFeel::GOLD,
        LiminalLookAndFeel::GHOST_WHITE
    };

    const float step = juce::MathConstants<float>::twoPi / 6.f;

    for (int eng = 0; eng < 3; ++eng)
    {
        if (! engines[eng]) continue;
        const float pulseAlpha = blend * 0.85f;

        for (int side = 0; side < 2; ++side)
        {
            const int   ptIdx = eng * 2 + side;
            const float angle = rotRad - juce::MathConstants<float>::halfPi + ptIdx * step;
            const juce::Point<float> tip { centre.x + outerR * std::cos (angle),
                                           centre.y + outerR * std::sin (angle) };
            // Shorter extension: 1.45× instead of 1.80× so rays don't overwhelm
            const juce::Point<float> ext { centre.x + outerR * 1.45f * std::cos (angle),
                                           centre.y + outerR * 1.45f * std::sin (angle) };
            g.setColour (engineColors[eng].withAlpha (pulseAlpha * 0.18f));
            g.drawLine ({ tip, ext }, 3.5f);
            g.setColour (engineColors[eng].withAlpha (pulseAlpha * 0.80f));
            g.drawLine ({ tip, ext }, 0.9f);
        }
    }
}

void ThresholdDisplay::setBlendLevel     (float v) { blendLevel.store (v); }
void ThresholdDisplay::setEnvelopeLevel  (float v) { envelopeLevel.store (v); }
void ThresholdDisplay::setThresholdValue (float v) { thresholdVal.store (v); }
void ThresholdDisplay::setSlewNorm       (float v) { slewNorm.store (v); }
void ThresholdDisplay::setDepthValue     (float v) { depthVal.store (v); }
void ThresholdDisplay::setToneValue      (float v) { toneVal.store (v); }
void ThresholdDisplay::setHauntActive    (bool v)  { hauntActive.store (v); }
void ThresholdDisplay::setShimmerActive  (bool v)  { shimmerActive.store (v); }
void ThresholdDisplay::setPitchGhostActive(bool v) { ghostActive.store (v); }
