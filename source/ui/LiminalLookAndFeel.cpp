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
                                            juce::Slider& /*slider*/)
{
    const float radius    = static_cast<float> (juce::jmin (width, height)) * 0.5f - 4.f;
    const float centreX   = static_cast<float> (x) + static_cast<float> (width)  * 0.5f;
    const float centreY   = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    const float thickness = 4.f;

    // Track ring
    {
        juce::Path track;
        track.addCentredArc (centreX, centreY, radius, radius,
                             0.f, startAngle, endAngle, true);
        g.setColour (KNOB_TRACK);
        g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // Gold filled arc
    {
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        juce::Path arc;
        arc.addCentredArc (centreX, centreY, radius, radius,
                           0.f, startAngle, angle, true);
        g.setColour (GOLD);
        g.strokePath (arc, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    // Center dot
    const float dotRadius = radius * 0.18f;
    g.setColour (COBALT);
    g.fillEllipse (centreX - dotRadius, centreY - dotRadius, dotRadius * 2.f, dotRadius * 2.f);
    g.setColour (GOLD);
    const float innerDot = dotRadius * 0.45f;
    g.fillEllipse (centreX - innerDot, centreY - innerDot, innerDot * 2.f, innerDot * 2.f);
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
