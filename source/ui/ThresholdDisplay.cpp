#include "ThresholdDisplay.h"
#include "LiminalLookAndFeel.h"

#include <cmath>

ThresholdDisplay::ThresholdDisplay()
{
    setOpaque (false);   // let nebula background show through
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

    // Threshold crossing detection: envelope just dropped below threshold
    const bool wasAbove = prevEnvelope >= displayThreshold;
    const bool nowBelow = newEnv < newThresh;
    if (wasAbove && nowBelow && flashIntensity < 0.1f)
    {
        flashIntensity = 1.f;
        rippleRadius   = 0.f;
        rippleActive   = true;
    }
    prevEnvelope = newEnv;

    // Decay flash
    flashIntensity = std::max (0.f, flashIntensity - 0.055f);  // ~18 frames = 0.3s at 60fps

    // Expand ripple ring
    if (rippleActive)
    {
        rippleRadius += 2.8f;
        if (rippleRadius > 90.f)
            rippleActive = false;
    }

    // Slow star rotation: ~9°/second at 60fps
    starRotation += 0.15f;
    if (starRotation >= 360.f) starRotation -= 360.f;

    displayBlend     = newBlend;
    displayEnvelope  = newEnv;
    displayThreshold = newThresh;

    repaint();
}

void ThresholdDisplay::resized() {}

void ThresholdDisplay::paint (juce::Graphics& g)
{
    const auto  bounds  = getLocalBounds().toFloat();
    const auto  centre  = bounds.getCentre();
    const float minDim  = std::min (bounds.getWidth(), bounds.getHeight());
    const float blend   = displayBlend;

    // Dark vignette over the nebula (makes the star pop without hiding background)
    {
        juce::ColourGradient vignette (juce::Colour (0x70050a1a), centre.x, centre.y,
                                       juce::Colour (0x00050a1a),
                                       centre.x + minDim * 0.5f, centre.y,
                                       true);
        g.setGradientFill (vignette);
        g.fillRect (bounds);
    }

    const float rotRad  = starRotation * juce::MathConstants<float>::pi / 180.f;
    const float starInner = minDim * 0.07f + blend * minDim * 0.06f;
    const float starOuter = minDim * 0.17f + blend * minDim * 0.22f;

    // ── Warm orange/gold glow behind the star ────────────────────────────────
    {
        const float gR1 = starOuter * (1.8f + blend * 1.2f);
        juce::ColourGradient g1 (juce::Colour (0x18ff8800), centre.x, centre.y,
                                  juce::Colour (0x00ff8800), centre.x + gR1, centre.y, true);
        g.setGradientFill (g1);
        g.fillEllipse (centre.x - gR1, centre.y - gR1, gR1 * 2.f, gR1 * 2.f);

        const float gR2 = starOuter * (1.1f + blend * 0.7f);
        juce::ColourGradient g2 (juce::Colour (0xff9900).withAlpha (0.15f + blend * 0.3f),
                                  centre.x, centre.y,
                                  juce::Colour (0x00ff9900), centre.x + gR2, centre.y, true);
        g.setGradientFill (g2);
        g.fillEllipse (centre.x - gR2, centre.y - gR2, gR2 * 2.f, gR2 * 2.f);

        const float gR3 = starOuter * 0.55f;
        juce::ColourGradient g3 (juce::Colour (0xffbb44).withAlpha (0.25f + blend * 0.55f),
                                  centre.x, centre.y,
                                  juce::Colour (0x00ffbb44), centre.x + gR3, centre.y, true);
        g.setGradientFill (g3);
        g.fillEllipse (centre.x - gR3, centre.y - gR3, gR3 * 2.f, gR3 * 2.f);
    }

    // ── Threshold crossing flash overlay ─────────────────────────────────────
    if (flashIntensity > 0.f)
    {
        const float flashR = minDim * 0.5f;
        juce::ColourGradient flash (juce::Colour (0xffffee88).withAlpha (flashIntensity * 0.28f),
                                    centre.x, centre.y,
                                    juce::Colour (0x00ffee88), centre.x + flashR, centre.y, true);
        g.setGradientFill (flash);
        g.fillRect (bounds);
    }

    // ── Threshold ring ────────────────────────────────────────────────────────
    const float thresholdRadius = minDim * 0.35f * (0.4f + displayThreshold * 0.5f);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.5f));
    g.drawEllipse (centre.x - thresholdRadius, centre.y - thresholdRadius,
                   thresholdRadius * 2.f, thresholdRadius * 2.f, 1.5f);

    // ── Envelope level ring ───────────────────────────────────────────────────
    const float envRadius = minDim * 0.35f * (0.4f + displayEnvelope * 0.5f);
    g.setColour (LiminalLookAndFeel::ICE_BLUE.withAlpha (0.4f));
    g.drawEllipse (centre.x - envRadius, centre.y - envRadius,
                   envRadius * 2.f, envRadius * 2.f, 1.5f);

    // ── Compass ring + cardinal ticks ─────────────────────────────────────────
    const float compassR = starOuter * 1.55f;
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.3f + blend * 0.2f));
    g.drawEllipse (centre.x - compassR, centre.y - compassR,
                   compassR * 2.f, compassR * 2.f, 0.75f);

    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.5f + blend * 0.25f));
    for (int i = 0; i < 4; ++i)
    {
        const float tickAngle = i * juce::MathConstants<float>::halfPi;
        const float tx0 = centre.x + std::sin (tickAngle) * (compassR - 3.5f);
        const float ty0 = centre.y - std::cos (tickAngle) * (compassR - 3.5f);
        const float tx1 = centre.x + std::sin (tickAngle) * (compassR + 3.5f);
        const float ty1 = centre.y - std::cos (tickAngle) * (compassR + 3.5f);
        g.drawLine (tx0, ty0, tx1, ty1, 1.2f);
    }

    // ── Compass rose: two overlapping 6-point stars ───────────────────────────
    // Primary star
    drawRotatedStar (g, centre, starInner, starOuter, 0.45f + blend * 0.55f, rotRad);
    // Secondary star at 30° offset (slightly smaller, creates 12-point compass rose)
    drawRotatedStar (g, centre,
                     starInner * 0.80f, starOuter * 0.82f,
                     (0.3f + blend * 0.4f),
                     rotRad + juce::MathConstants<float>::pi / 6.f);

    // ── Per-engine axis pulses ────────────────────────────────────────────────
    drawEngineAxes (g, centre, starOuter, blend, rotRad);

    // ── Threshold crossing ripple ─────────────────────────────────────────────
    if (rippleActive && rippleRadius > 0.f)
    {
        const float rippleAlpha = 0.6f * (1.f - rippleRadius / 90.f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (rippleAlpha));
        g.drawEllipse (centre.x - rippleRadius, centre.y - rippleRadius,
                       rippleRadius * 2.f, rippleRadius * 2.f, 1.5f);
    }
}

void ThresholdDisplay::drawRotatedStar (juce::Graphics& g,
                                         juce::Point<float> centre,
                                         float innerR, float outerR,
                                         float alpha, float rotRad)
{
    static constexpr int kPoints = 6;
    const float step = juce::MathConstants<float>::twoPi / static_cast<float> (kPoints);

    juce::Path star;
    for (int i = 0; i < kPoints; ++i)
    {
        const float outerAngle = rotRad - juce::MathConstants<float>::halfPi + i * step;
        const float innerAngle = outerAngle + step * 0.5f;

        const juce::Point<float> outerPt { centre.x + outerR * std::cos (outerAngle),
                                            centre.y + outerR * std::sin (outerAngle) };
        const juce::Point<float> innerPt { centre.x + innerR * std::cos (innerAngle),
                                            centre.y + innerR * std::sin (innerAngle) };
        if (i == 0) star.startNewSubPath (outerPt);
        else        star.lineTo (outerPt);
        star.lineTo (innerPt);
    }
    star.closeSubPath();

    // Star interior fill: warm orange-gold radial gradient
    {
        juce::ColourGradient fill (juce::Colour (0xffcc8833).withAlpha (alpha * 0.35f),
                                    centre.x, centre.y,
                                    juce::Colour (0x00cc8833),
                                    centre.x + outerR, centre.y, true);
        g.setGradientFill (fill);
        g.fillPath (star);
    }

    // Glow layer (thick, transparent)
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha * 0.25f));
    g.strokePath (star, juce::PathStrokeType (4.f));

    // Core layer (thin, bright)
    g.setColour (juce::Colour (0xffd4a84c).withAlpha (alpha));
    g.strokePath (star, juce::PathStrokeType (1.2f));
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

        const float pulseAlpha = blend * 0.75f;
        g.setColour (engineColors[eng].withAlpha (pulseAlpha));

        for (int side = 0; side < 2; ++side)
        {
            const int   ptIdx = eng * 2 + side;
            const float angle = rotRad - juce::MathConstants<float>::halfPi + ptIdx * step;

            const juce::Point<float> tip {
                centre.x + outerR * std::cos (angle),
                centre.y + outerR * std::sin (angle)
            };
            const juce::Point<float> ext {
                centre.x + outerR * 1.6f * std::cos (angle),
                centre.y + outerR * 1.6f * std::sin (angle)
            };
            // Glow line
            g.setColour (engineColors[eng].withAlpha (pulseAlpha * 0.35f));
            g.drawLine ({ tip, ext }, 3.f);
            // Core line
            g.setColour (engineColors[eng].withAlpha (pulseAlpha));
            g.drawLine ({ tip, ext }, 1.f);
        }
    }
}

void ThresholdDisplay::setBlendLevel     (float v) { blendLevel.store (v); }
void ThresholdDisplay::setEnvelopeLevel  (float v) { envelopeLevel.store (v); }
void ThresholdDisplay::setThresholdValue (float v) { thresholdVal.store (v); }
void ThresholdDisplay::setHauntActive    (bool v)  { hauntActive.store (v); }
void ThresholdDisplay::setShimmerActive  (bool v)  { shimmerActive.store (v); }
void ThresholdDisplay::setPitchGhostActive(bool v) { ghostActive.store (v); }
