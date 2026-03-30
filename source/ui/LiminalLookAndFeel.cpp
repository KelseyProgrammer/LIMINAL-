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
    setColour (juce::ComboBox::textColourId,              TEXT_PRIMARY);
    setColour (juce::ComboBox::outlineColourId,           PANEL_BORDER);
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
        const float glowAlpha = slider.isMouseButtonDown() ? 0.35f : 0.18f;
        const float glowR = radius + 9.f;
        juce::ColourGradient hoverGrad (GOLD.withAlpha (glowAlpha), centreX, centreY,
                                        GOLD.withAlpha (0.f), centreX + glowR, centreY,
                                        true);
        g.setGradientFill (hoverGrad);
        g.fillEllipse (centreX - glowR, centreY - glowR, glowR * 2.f, glowR * 2.f);
    }

    // ── Outer dark rim ───────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xff1a0e04));
    g.fillEllipse (centreX - radius - 2.f, centreY - radius - 2.f,
                   (radius + 2.f) * 2.f, (radius + 2.f) * 2.f);

    // ── Metallic sphere (warm gold, upper-left highlight → lower-right shadow) ──
    {
        juce::ColourGradient sphere (
            juce::Colour (0xffd4a050),
            centreX - radius * 0.3f, centreY - radius * 0.35f,
            juce::Colour (0xff2a1800),
            centreX + radius * 0.7f, centreY + radius * 0.7f,
            false);
        sphere.addColour (0.35, juce::Colour (0xffc9a84c));
        sphere.addColour (0.65, juce::Colour (0xff7a5018));
        g.setGradientFill (sphere);
        g.fillEllipse (centreX - radius, centreY - radius, radius * 2.f, radius * 2.f);
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

    // ── Value indicator dot on rim ────────────────────────────────────────────
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float indicatorR = radius - 4.5f;
    const float dotX = centreX + std::sin (angle) * indicatorR;
    const float dotY = centreY - std::cos (angle) * indicatorR;
    const float dotR = juce::jmax (2.f, radius * 0.1f);

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

void LiminalLookAndFeel::drawToggleButton (juce::Graphics& g,
                                            juce::ToggleButton& button,
                                            bool highlighted, bool /*down*/)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (2.f);
    const bool on     = button.getToggleState();

    g.setColour (on ? GOLD : PANEL_BORDER);
    g.fillRoundedRectangle (bounds, 3.f);

    if (highlighted)
    {
        g.setColour (on ? GOLD.brighter (0.2f) : GOLD_DIM);
        g.drawRoundedRectangle (bounds, 3.f, 1.f);
    }

    g.setColour (on ? COBALT : TEXT_DIM);
    g.setFont (juce::Font (juce::FontOptions().withHeight (10.f)));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds(),
                      juce::Justification::centred, 1);
}
