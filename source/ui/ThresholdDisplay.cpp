#include "ThresholdDisplay.h"
#include "LiminalLookAndFeel.h"

#include <cmath>

// Geometry of the painted star in the background artwork, in this
// component's local coordinates (bounds are centred on the painted star):
//   centre        = (width/2, height/2)
//   hexagram tips ≈ 50 px
//   compass ring  ≈ 87 px
namespace
{
    constexpr float kStarBaseR    = 50.f;
    constexpr float kCompassR     = 87.f;
    constexpr float kRingMinR     = 58.f;   // threshold ring at threshold = 0
    constexpr float kRingSpanR    = 46.f;   // ring travel up to threshold = 1
}

ThresholdDisplay::ThresholdDisplay()
{
    setOpaque (false);
    startTimerHz (60);
}

ThresholdDisplay::~ThresholdDisplay()
{
    stopTimer();
}

float ThresholdDisplay::thresholdToRadius (float threshold)
{
    return kRingMinR + threshold * kRingSpanR;
}

float ThresholdDisplay::radiusToThreshold (float radius) const
{
    return juce::jlimit (0.f, 1.f, (radius - kRingMinR) / kRingSpanR);
}

void ThresholdDisplay::mouseDown (const juce::MouseEvent& e)
{
    draggingThreshold = true;
    if (onThresholdDragStart) onThresholdDragStart();
    mouseDrag (e);
}

void ThresholdDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingThreshold) return;

    const auto centre = getLocalBounds().toFloat().getCentre();
    const float r = e.position.getDistanceFrom (centre);
    if (onThresholdDrag) onThresholdDrag (radiusToThreshold (r));
}

void ThresholdDisplay::mouseUp (const juce::MouseEvent&)
{
    if (draggingThreshold && onThresholdDragEnd) onThresholdDragEnd();
    draggingThreshold = false;
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

    // Idle breathing — keeps the card subtly alive even in full dry signal
    breathePhase += 0.025f;
    if (breathePhase > juce::MathConstants<float>::twoPi)
        breathePhase -= juce::MathConstants<float>::twoPi;

    displayBlend     = newBlend;
    displayEnvelope  = newEnv;
    displayThreshold = newThresh;

    repaint();
}

void ThresholdDisplay::resized() {}

// Draw a hexagram (Star of David) outline.
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

// Rays radiating from the 6 hexagram tips.
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
    const float blend   = displayBlend;
    const float rotRad  = starRotation * juce::MathConstants<float>::pi / 180.f;
    const float breathe = 0.5f + 0.5f * std::sin (breathePhase);

    // ── Knob-driven display parameters ────────────────────────────────────────
    // DEPTH (0–1): scales star size and overall glow brightness.
    const float depthScale = 0.70f + displayDepth * 0.30f;

    // TONE (−1…+1): interpolates star/ray colour between cool and warm.
    const float toneT = (displayTone + 1.f) * 0.5f;
    const juce::Colour toneColour =
        (toneT < 0.5f)
        ? LiminalLookAndFeel::ICE_BLUE.interpolatedWith (LiminalLookAndFeel::GOLD, toneT * 2.f)
        : LiminalLookAndFeel::GOLD.interpolatedWith     (juce::Colour (0xfffce8a0), (toneT - 0.5f) * 2.f);

    const juce::Colour glowColour =
        (toneT < 0.5f)
        ? juce::Colour (0xff3355bb).interpolatedWith (juce::Colour (0xff884400), toneT * 2.f)
        : juce::Colour (0xff884400).interpolatedWith (juce::Colour (0xffcc7700), (toneT - 0.5f) * 2.f);

    // ── Breathing ambient glow over the painted star ─────────────────────────
    // Mostly blend-driven; a faint idle pulse keeps the star alive when dry.
    {
        const float gd   = depthScale;
        const float idle = 0.030f + breathe * 0.030f;

        const float r1 = 95.f + blend * 45.f;
        juce::ColourGradient ga (glowColour.withAlpha ((idle + blend * 0.16f) * gd),
                                  centre.x, centre.y,
                                  glowColour.withAlpha (0.f), centre.x + r1, centre.y, true);
        g.setGradientFill (ga); g.fillEllipse (centre.x-r1, centre.y-r1, r1*2, r1*2);

        const float r2 = 55.f + blend * 28.f;
        juce::ColourGradient gb (glowColour.withAlpha ((idle + blend * 0.30f) * gd),
                                  centre.x, centre.y,
                                  glowColour.withAlpha (0.f), centre.x + r2, centre.y, true);
        g.setGradientFill (gb); g.fillEllipse (centre.x-r2, centre.y-r2, r2*2, r2*2);

        const float r3 = 26.f + blend * 16.f;
        juce::ColourGradient gc (glowColour.withAlpha ((idle * 1.5f + blend * 0.45f) * gd),
                                  centre.x, centre.y,
                                  glowColour.withAlpha (0.f), centre.x + r3, centre.y, true);
        g.setGradientFill (gc); g.fillEllipse (centre.x-r3, centre.y-r3, r3*2, r3*2);
    }

    // ── Downward warm exhale — only while engines are awake ──────────────────
    if (blend > 0.01f)
    {
        const float burstW = 130.f + blend * 70.f;
        const float burstH = 130.f + blend * 60.f;
        const float alpha  = blend * 0.30f;

        juce::ColourGradient burst (juce::Colour (0xff7700).withAlpha (alpha * 0.65f),
                                    centre.x, centre.y,
                                    juce::Colour (0x00ff5500),
                                    centre.x, centre.y + burstH, false);
        burst.addColour (0.4, juce::Colour (0xff5500).withAlpha (alpha * 0.28f));
        g.setGradientFill (burst);
        g.fillEllipse (centre.x - burstW * 0.5f, centre.y - 8.f, burstW, burstH);
    }

    // ── Threshold crossing flash ──────────────────────────────────────────────
    if (flashIntensity > 0.f)
    {
        const float flashR = 120.f;
        juce::ColourGradient flash (juce::Colour (0xffffee88).withAlpha (flashIntensity * 0.32f),
                                    centre.x, centre.y,
                                    juce::Colour (0x00ffee88), centre.x + flashR, centre.y, true);
        g.setGradientFill (flash); g.fillRect (bounds);
    }

    // ── Star geometry — DEPTH scales size, blend makes it bloom ──────────────
    const float outerR = kStarBaseR * depthScale * (1.f + blend * 0.16f);
    const float innerR = outerR * 0.375f;
    const float coreR  = outerR * 0.26f;
    const float rayEnd = outerR * (1.55f + blend * 0.45f);

    // ── Rotating compass ticks on the painted ring ────────────────────────────
    {
        const float tickAlpha = 0.16f + blend * 0.55f;
        for (int i = 0; i < 24; ++i)
        {
            const float a = rotRad * 0.5f + i * juce::MathConstants<float>::twoPi / 24.f;
            const bool  major = (i % 6 == 0);
            const float len   = major ? 7.f : 3.5f;
            g.setColour (LiminalLookAndFeel::GOLD.withAlpha (tickAlpha * (major ? 1.f : 0.55f)));
            g.drawLine (centre.x + std::cos(a) * (kCompassR - len),
                        centre.y + std::sin(a) * (kCompassR - len),
                        centre.x + std::cos(a) * kCompassR,
                        centre.y + std::sin(a) * kCompassR,
                        major ? 1.4f : 0.7f);
        }
    }

    // ── Threshold ring (gold, draggable) ──────────────────────────────────────
    const float threshR = thresholdToRadius (displayThreshold);
    {
        const float ringAlpha = draggingThreshold ? 0.95f : (0.45f + blend * 0.25f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (ringAlpha));
        g.drawEllipse (centre.x - threshR, centre.y - threshR,
                       threshR * 2.f, threshR * 2.f, draggingThreshold ? 2.2f : 1.5f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (ringAlpha * 0.35f));
        g.drawEllipse (centre.x - threshR - 3.f, centre.y - threshR - 3.f,
                       (threshR + 3.f) * 2.f, (threshR + 3.f) * 2.f, 0.7f);

        // Drag handle dots at the cardinal points while dragging
        if (draggingThreshold)
        {
            g.setColour (juce::Colour (0xffffe8a0));
            for (int i = 0; i < 4; ++i)
            {
                const float a = i * juce::MathConstants<float>::halfPi;
                g.fillEllipse (centre.x + std::cos(a) * threshR - 2.5f,
                               centre.y + std::sin(a) * threshR - 2.5f, 5.f, 5.f);
            }
        }
    }

    // ── Envelope ring (ice blue, moves with the signal) ───────────────────────
    {
        const float envR = thresholdToRadius (juce::jlimit (0.f, 1.f, displayEnvelope));
        g.setColour (LiminalLookAndFeel::ICE_BLUE.withAlpha (0.38f));
        g.drawEllipse (centre.x - envR, centre.y - envR,
                       envR * 2.f, envR * 2.f, 1.1f);
    }

    // ── Rays from the hexagram tips — TONE sets the colour ────────────────────
    if (blend > 0.005f)
    {
        drawHexRays (g, centre.x, centre.y, outerR * 0.95f, rayEnd, rotRad,
                     toneColour.withAlpha (blend * 0.18f), 4.5f);
        drawHexRays (g, centre.x, centre.y, outerR * 0.95f, rayEnd, rotRad,
                     toneColour.withAlpha (blend * 0.65f), 1.0f);
    }

    // ── Hexagram overlay — wakes over the painted star as blend rises ────────
    {
        const float alpha = 0.06f + breathe * 0.05f + blend * 0.80f;

        if (blend > 0.01f)
        {
            const juce::Colour fillCol = juce::Colour (0xffcc7700).withAlpha (blend * 0.16f);
            drawHexagram (g, centre.x, centre.y, outerR, innerR, rotRad,
                          juce::Colours::transparentBlack, 0, fillCol);
        }

        drawHexagram (g, centre.x, centre.y, outerR, innerR, rotRad,
                      toneColour.withAlpha (alpha * 0.22f), 4.5f);
        drawHexagram (g, centre.x, centre.y, outerR, innerR, rotRad,
                      toneColour.withAlpha (juce::jmin (1.f, alpha)), 1.3f);
    }

    // ── Inner 4-pointed star (counter-rotating) ───────────────────────────────
    if (blend > 0.01f)
    {
        const float innerAlpha = blend * 0.9f;
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

        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (innerAlpha * 0.35f));
        g.strokePath (star4, juce::PathStrokeType (4.5f));
        g.setColour (juce::Colour (0xfffce8a0).withAlpha (innerAlpha));
        g.strokePath (star4, juce::PathStrokeType (1.2f));
    }

    // ── Centre bright point — pulses with breathe + blend ────────────────────
    {
        const float dotA = 0.18f + breathe * 0.12f + blend * 0.70f;
        const float dotR = 2.f + blend * 2.5f;
        juce::ColourGradient cg (juce::Colour (0xfffce8a0).withAlpha (dotA),
                                  centre.x, centre.y,
                                  juce::Colour (0x00ffe880),
                                  centre.x + 10.f + blend * 8.f, centre.y, true);
        g.setGradientFill (cg);
        g.fillEllipse (centre.x - 12.f, centre.y - 12.f, 24.f, 24.f);
        g.setColour (juce::Colours::white.withAlpha (juce::jmin (1.f, dotA)));
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
