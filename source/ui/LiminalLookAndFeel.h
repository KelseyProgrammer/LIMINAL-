#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class LiminalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LiminalLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider& slider) override;

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool highlighted, bool down) override;

    void setLFOPhase (float phase) noexcept { lfoPhase = phase; }

    // ── Color Palette ──────────────────────────────────────────
    static const juce::Colour COBALT;
    static const juce::Colour COBALT_MID;
    static const juce::Colour GOLD;
    static const juce::Colour GOLD_DIM;
    static const juce::Colour ICE_BLUE;
    static const juce::Colour GHOST_WHITE;
    static const juce::Colour PANEL_BORDER;
    static const juce::Colour KNOB_TRACK;
    static const juce::Colour TEXT_PRIMARY;
    static const juce::Colour TEXT_DIM;

private:
    float lfoPhase = 0.f;
};
