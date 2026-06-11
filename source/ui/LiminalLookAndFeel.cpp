#include "LiminalLookAndFeel.h"

const juce::Colour LiminalLookAndFeel::COBALT      (0xff0a0f2e);
const juce::Colour LiminalLookAndFeel::COBALT_MID  (0xff1a2050);
const juce::Colour LiminalLookAndFeel::GOLD        (0xffc9a84c);
const juce::Colour LiminalLookAndFeel::GOLD_DIM    (0xff6b5a28);
const juce::Colour LiminalLookAndFeel::ICE_BLUE    (0xffa8c4e8);
const juce::Colour LiminalLookAndFeel::GHOST_WHITE (0xffe8e8f0);
const juce::Colour LiminalLookAndFeel::PANEL_BORDER(0xff2a3060);
const juce::Colour LiminalLookAndFeel::KNOB_TRACK  (0xff1e2448);
const juce::Colour LiminalLookAndFeel::TEXT_PRIMARY(0xffe8e8f0);
const juce::Colour LiminalLookAndFeel::TEXT_DIM    (0xff6070a0);

LiminalLookAndFeel::LiminalLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, COBALT);
    setColour (juce::Slider::thumbColourId,               GOLD);
    setColour (juce::Slider::trackColourId,               KNOB_TRACK);
    setColour (juce::Label::textColourId,                 TEXT_PRIMARY);
    setColour (juce::TextButton::buttonColourId,          PANEL_BORDER);
    setColour (juce::TextButton::textColourOffId,         TEXT_PRIMARY);
    setColour (juce::ComboBox::backgroundColourId,        COBALT_MID);
    setColour (juce::ComboBox::textColourId,              GOLD);
    setColour (juce::ComboBox::outlineColourId,           PANEL_BORDER);
    setColour (juce::PopupMenu::backgroundColourId,       juce::Colour (0xf2081020));
    setColour (juce::PopupMenu::textColourId,             GHOST_WHITE);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, GOLD_DIM);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xf2081020));
    setColour (juce::BubbleComponent::outlineColourId,    GOLD_DIM);
    setColour (juce::TooltipWindow::textColourId,         ICE_BLUE);
}

void LiminalLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                            int x, int y, int width, int height,
                                            float sliderPos,
                                            float startAngle, float endAngle,
                                            juce::Slider& slider)
{
    const float radius  = static_cast<float> (juce::jmin (width, height)) * 0.5f - 4.f;
    const float centreX = static_cast<float> (x) + static_cast<float> (width)  * 0.5f;
    const float centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;

    if (radius < 4.f) return;

    // ── Hover / click glow ───────────────────────────────────────────────────
    if (slider.isMouseOver() || slider.isMouseButtonDown())
    {
        const float glowAlpha = slider.isMouseButtonDown() ? 0.40f : 0.20f;
        const float glowR = radius + 10.f;
        juce::ColourGradient hoverGrad (GOLD.withAlpha (glowAlpha), centreX, centreY,
                                        GOLD.withAlpha (0.f), centreX + glowR, centreY,
                                        true);
        g.setGradientFill (hoverGrad);
        g.fillEllipse (centreX - glowR, centreY - glowR, glowR * 2.f, glowR * 2.f);
    }

    // ── Dark bezel ring (covers the painted knob beneath) ────────────────────
    g.setColour (juce::Colour (0xff0c0a14));
    g.fillEllipse (centreX - radius - 4.f, centreY - radius - 4.f,
                   (radius + 4.f) * 2.f, (radius + 4.f) * 2.f);
    g.setColour (juce::Colour (0xff3a3020));
    g.drawEllipse (centreX - radius - 3.f, centreY - radius - 3.f,
                   (radius + 3.f) * 2.f, (radius + 3.f) * 2.f, 1.f);

    // ── Metallic sphere (warm gold, upper-left highlight → lower-right shadow) ──
    {
        juce::ColourGradient sphere (
            juce::Colour (0xffe2b562),
            centreX - radius * 0.3f, centreY - radius * 0.35f,
            juce::Colour (0xff2a1800),
            centreX + radius * 0.7f, centreY + radius * 0.7f,
            false);
        sphere.addColour (0.35, juce::Colour (0xffd2af55));
        sphere.addColour (0.65, juce::Colour (0xff7a5018));
        g.setGradientFill (sphere);
        g.fillEllipse (centreX - radius, centreY - radius, radius * 2.f, radius * 2.f);
    }

    // Brushed-metal arcs (subtle concentric texture)
    {
        g.setColour (juce::Colour (0x18ffe8a0));
        for (float rr = radius * 0.35f; rr < radius * 0.9f; rr += radius * 0.18f)
            g.drawEllipse (centreX - rr, centreY - rr, rr * 2.f, rr * 2.f, 0.5f);
    }

    // Warm highlight sheen (upper-left)
    {
        const float sheenR = radius * 0.62f;
        juce::ColourGradient sheen (
            juce::Colour (0x50ffe8a0),
            centreX - radius * 0.38f, centreY - radius * 0.48f,
            juce::Colour (0x00ffe8a0),
            centreX + radius * 0.1f,  centreY + radius * 0.08f,
            false);
        g.setGradientFill (sheen);
        g.fillEllipse (centreX - sheenR, centreY - sheenR, sheenR * 2.f, sheenR * 2.f);
    }

    // Edge darkening (rim vignette)
    {
        juce::ColourGradient rim (
            juce::Colour (0x00000000), centreX, centreY,
            juce::Colour (0x70000000), centreX + radius, centreY,
            true);
        g.setGradientFill (rim);
        g.fillEllipse (centreX - radius, centreY - radius, radius * 2.f, radius * 2.f);
    }

    // ── Pointer: dark slit + bright dot at the rim ────────────────────────────
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float dirX  = std::sin (angle);
    const float dirY  = -std::cos (angle);

    g.setColour (juce::Colour (0xb01a0e04));
    g.drawLine (centreX + dirX * radius * 0.45f, centreY + dirY * radius * 0.45f,
                centreX + dirX * (radius - 3.f), centreY + dirY * (radius - 3.f),
                juce::jmax (1.2f, radius * 0.07f));

    const float indicatorR = radius - 4.5f;
    const float dotX = centreX + dirX * indicatorR;
    const float dotY = centreY + dirY * indicatorR;
    const float dotR = juce::jmax (2.f, radius * 0.10f);

    g.setColour (juce::Colour (0x50ffe880));
    g.fillEllipse (dotX - dotR * 1.8f, dotY - dotR * 1.8f, dotR * 3.6f, dotR * 3.6f);
    g.setColour (juce::Colour (0xffffd070));
    g.fillEllipse (dotX - dotR, dotY - dotR, dotR * 2.f, dotR * 2.f);

    // ── LFO phase orbiting dot (mod knobs only) ───────────────────────────────
    const juce::String& sliderName = slider.getName();
    const bool isModKnob = sliderName.startsWith ("lfo") || sliderName.startsWith ("env");
    if (isModKnob)
    {
        const float lfoAngle = lfoPhase * juce::MathConstants<float>::twoPi;
        const float orbitR   = radius + 6.f;
        const float lfoX = centreX + std::sin (lfoAngle) * orbitR;
        const float lfoY = centreY - std::cos (lfoAngle) * orbitR;
        const float lfoR = 2.f;

        g.setColour (ICE_BLUE.withAlpha (0.15f));
        g.drawEllipse (centreX - orbitR, centreY - orbitR, orbitR * 2.f, orbitR * 2.f, 0.75f);

        g.setColour (ICE_BLUE.withAlpha (0.35f));
        g.fillEllipse (lfoX - lfoR * 2.f, lfoY - lfoR * 2.f, lfoR * 4.f, lfoR * 4.f);
        g.setColour (ICE_BLUE);
        g.fillEllipse (lfoX - lfoR, lfoY - lfoR, lfoR * 2.f, lfoR * 2.f);
    }
}

void LiminalLookAndFeel::drawLinearSlider (juce::Graphics& g,
                                            int x, int y, int width, int height,
                                            float sliderPos,
                                            float /*minSliderPos*/, float /*maxSliderPos*/,
                                            juce::Slider::SliderStyle /*style*/,
                                            juce::Slider& slider)
{
    // Ramp morph slider: redraw the whole track (covering the painted one)
    // and a glowing gold thumb at the morph position.
    const float cy = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    const float x0 = static_cast<float> (x) + 6.f;
    const float x1 = static_cast<float> (x + width) - 6.f;

    // Track bed: dark bar that hides the painted static thumb
    g.setColour (juce::Colour (0xff0a1226));
    g.fillRoundedRectangle (x0 - 4.f, cy - 4.f, (x1 - x0) + 8.f, 8.f, 4.f);

    // Track lines
    g.setColour (GOLD_DIM.withAlpha (0.85f));
    g.drawLine (x0, cy, x1, cy, 1.2f);
    g.setColour (GOLD.withAlpha (0.18f));
    g.drawLine (x0, cy + 2.5f, x1, cy + 2.5f, 0.6f);

    // Progress fill from A side to thumb
    g.setColour (GOLD.withAlpha (0.55f));
    g.drawLine (x0, cy, sliderPos, cy, 1.6f);

    // Thumb: gold orb with glow
    const float tr = 5.5f;
    const bool engaged = slider.isMouseOver() || slider.isMouseButtonDown();
    juce::ColourGradient glow (GOLD.withAlpha (engaged ? 0.55f : 0.35f), sliderPos, cy,
                               GOLD.withAlpha (0.f), sliderPos + tr * 3.f, cy, true);
    g.setGradientFill (glow);
    g.fillEllipse (sliderPos - tr * 3.f, cy - tr * 3.f, tr * 6.f, tr * 6.f);

    juce::ColourGradient orb (juce::Colour (0xffffe8a0), sliderPos - tr * 0.4f, cy - tr * 0.4f,
                              juce::Colour (0xff8a6418), sliderPos + tr, cy + tr, false);
    g.setGradientFill (orb);
    g.fillEllipse (sliderPos - tr, cy - tr, tr * 2.f, tr * 2.f);
    g.setColour (juce::Colour (0xff2a1c04));
    g.drawEllipse (sliderPos - tr, cy - tr, tr * 2.f, tr * 2.f, 0.8f);
}

// Shared capsule for LATCH / INV toggles and the A / B buttons — matches the
// small rounded capsules painted in the artwork.
static void drawCapsule (juce::Graphics& g, juce::Rectangle<float> bounds,
                          bool on, bool highlighted)
{
    const float corner = 4.f;

    g.setColour (on ? LiminalLookAndFeel::GOLD
                    : juce::Colour (0xd0101830));
    g.fillRoundedRectangle (bounds, corner);

    if (on)
    {
        juce::ColourGradient sheen (juce::Colour (0x60ffffff),
                                    bounds.getX(), bounds.getY(),
                                    juce::Colour (0x00ffffff),
                                    bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (sheen);
        g.fillRoundedRectangle (bounds, corner);
    }

    g.setColour (highlighted ? juce::Colour (0xffe8d8a8)
                             : juce::Colour (0xffcab070).withAlpha (0.9f));
    g.drawRoundedRectangle (bounds, corner, 1.1f);
}

void LiminalLookAndFeel::drawToggleButton (juce::Graphics& g,
                                            juce::ToggleButton& button,
                                            bool highlighted, bool /*down*/)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.f);
    const bool on     = button.getToggleState();

    drawCapsule (g, bounds, on, highlighted);

    g.setColour (on ? COBALT : juce::Colour (0xffd8cba0));
    g.setFont (juce::Font (juce::FontOptions().withHeight (9.5f).withStyle ("Bold")));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds(),
                      juce::Justification::centred, 1);
}

void LiminalLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& /*backgroundColour*/,
                                                bool highlighted, bool down)
{
    drawCapsule (g, button.getLocalBounds().toFloat().reduced (1.f), down, highlighted);
}

void LiminalLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                          bool /*highlighted*/, bool down)
{
    g.setColour (down ? COBALT : juce::Colour (0xffd8cba0));
    g.setFont (juce::Font (juce::FontOptions().withHeight (10.f).withStyle ("Bold")));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds(),
                      juce::Justification::centred, 1);
}

void LiminalLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                        bool /*isButtonDown*/,
                                        int /*buttonX*/, int /*buttonY*/,
                                        int /*buttonW*/, int /*buttonH*/,
                                        juce::ComboBox& /*box*/)
{
    const auto bounds = juce::Rectangle<float> (0.f, 0.f,
                                                static_cast<float> (width),
                                                static_cast<float> (height)).reduced (0.5f);
    g.setColour (juce::Colour (0xc0081020));
    g.fillRoundedRectangle (bounds, 3.f);
    g.setColour (GOLD_DIM);
    g.drawRoundedRectangle (bounds, 3.f, 1.f);

    // Small gold arrow at right
    const float ax = static_cast<float> (width) - 11.f;
    const float ay = static_cast<float> (height) * 0.5f - 1.5f;
    juce::Path arrow;
    arrow.addTriangle (ax, ay, ax + 6.f, ay, ax + 3.f, ay + 4.f);
    g.setColour (GOLD);
    g.fillPath (arrow);
}
