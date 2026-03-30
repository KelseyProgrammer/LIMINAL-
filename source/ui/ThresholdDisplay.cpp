#include "ThresholdDisplay.h"
#include "../ui/LiminalLookAndFeel.h"

#include <cmath>

ThresholdDisplay::ThresholdDisplay()
{
    startTimerHz (60);
}

ThresholdDisplay::~ThresholdDisplay()
{
    stopTimer();
}

void ThresholdDisplay::timerCallback()
{
    displayBlend     = blendLevel.load();
    displayEnvelope  = envelopeLevel.load();
    displayThreshold = thresholdVal.load();
    repaint();
}

void ThresholdDisplay::resized()
{
}

void ThresholdDisplay::paint (juce::Graphics& g)
{
    const auto bounds  = getLocalBounds().toFloat();
    const auto centre  = bounds.getCentre();
    const float minDim = std::min (bounds.getWidth(), bounds.getHeight());

    // Background radial gradient
    {
        juce::ColourGradient grad (LiminalLookAndFeel::COBALT_MID, centre.x, centre.y,
                                   LiminalLookAndFeel::COBALT,
                                   centre.x + minDim * 0.5f, centre.y,
                                   true);
        g.setGradientFill (grad);
        g.fillRect (bounds);
    }

    // Threshold ring
    const float thresholdRadius = minDim * 0.35f * (0.4f + displayThreshold * 0.5f);
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (0.5f));
    g.drawEllipse (centre.x - thresholdRadius, centre.y - thresholdRadius,
                   thresholdRadius * 2.f, thresholdRadius * 2.f, 1.5f);

    // Envelope level ring (moves inward as signal drops)
    const float envRadius = minDim * 0.35f * (0.4f + displayEnvelope * 0.5f);
    g.setColour (LiminalLookAndFeel::ICE_BLUE.withAlpha (0.4f));
    g.drawEllipse (centre.x - envRadius, centre.y - envRadius,
                   envRadius * 2.f, envRadius * 2.f, 1.5f);

    // Central star (scales with blend)
    const float starInner = minDim * 0.08f + displayBlend * minDim * 0.08f;
    const float starOuter = minDim * 0.15f + displayBlend * minDim * 0.18f;
    drawStar (g, centre, starInner, starOuter, displayBlend);
}

void ThresholdDisplay::drawStar (juce::Graphics& g,
                                  juce::Point<float> centre,
                                  float innerR, float outerR,
                                  float blendLevel)
{
    // Six-pointed star: 6 outer points alternating with 6 inner points
    // Points at 0°, 60°, 120°, 180°, 240°, 300°
    static constexpr int kPoints = 6;
    const float angleStep = juce::MathConstants<float>::twoPi / static_cast<float> (kPoints);
    const float startAngle = -juce::MathConstants<float>::halfPi;

    juce::Path starPath;

    for (int i = 0; i < kPoints; ++i)
    {
        const float outerAngle = startAngle + i * angleStep;
        const float innerAngle = outerAngle + angleStep * 0.5f;

        const juce::Point<float> outerPt {
            centre.x + outerR * std::cos (outerAngle),
            centre.y + outerR * std::sin (outerAngle)
        };
        const juce::Point<float> innerPt {
            centre.x + innerR * std::cos (innerAngle),
            centre.y + innerR * std::sin (innerAngle)
        };

        if (i == 0)
            starPath.startNewSubPath (outerPt);
        else
            starPath.lineTo (outerPt);

        starPath.lineTo (innerPt);
    }
    starPath.closeSubPath();

    const float alpha = 0.4f + blendLevel * 0.6f;

    // Glow layer (thick, transparent)
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha * 0.3f));
    g.strokePath (starPath, juce::PathStrokeType (4.f));

    // Core layer (thin, opaque)
    g.setColour (LiminalLookAndFeel::GOLD.withAlpha (alpha));
    g.strokePath (starPath, juce::PathStrokeType (1.5f));

    // Per-engine axis coloring
    const bool engines[3] = {
        hauntActive.load(),
        shimmerActive.load(),
        ghostActive.load()
    };
    const juce::Colour engineColors[3] = {
        LiminalLookAndFeel::ICE_BLUE,
        LiminalLookAndFeel::GOLD,
        LiminalLookAndFeel::GHOST_WHITE
    };

    // Each engine gets a pair of opposite star axes
    for (int eng = 0; eng < 3; ++eng)
    {
        if (!engines[eng]) continue;

        const float pulseAlpha = blendLevel * 0.8f;
        g.setColour (engineColors[eng].withAlpha (pulseAlpha));

        for (int side = 0; side < 2; ++side)
        {
            const int ptIdx = eng * 2 + side;
            const float angle = startAngle + ptIdx * angleStep;
            const juce::Point<float> tip {
                centre.x + outerR * std::cos (angle),
                centre.y + outerR * std::sin (angle)
            };
            // Radial line from centre outward past the tip
            const juce::Point<float> ext {
                centre.x + outerR * 1.5f * std::cos (angle),
                centre.y + outerR * 1.5f * std::sin (angle)
            };
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
