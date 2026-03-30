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
      knobRampTime  ("rampTime",  "TIME",      p.apvts)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (thresholdDisplay);

    addAndMakeVisible (knobThreshold);
    addAndMakeVisible (knobSlew);
    addAndMakeVisible (knobDepth);
    addAndMakeVisible (knobTone);
    addAndMakeVisible (knobMix);

    addAndMakeVisible (hauntPanel);
    addAndMakeVisible (shimmerPanel);
    addAndMakeVisible (ghostPanel);

    rampSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    rampSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    rampSlider.setRange (0.0, 1.0);
    addAndMakeVisible (rampSlider);

    addAndMakeVisible (latchButton);
    addAndMakeVisible (knobRampTime);

    setSize (800, 500);
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
    stopTimer();
}

void PluginEditor::timerCallback()
{
    const float blend    = processorRef.getBlendLevel();
    const float envelope = processorRef.getEnvelopeLevel();
    const float threshold = processorRef.apvts.getRawParameterValue ("threshold")->load();

    thresholdDisplay.setBlendLevel     (blend);
    thresholdDisplay.setEnvelopeLevel  (envelope);
    thresholdDisplay.setThresholdValue (threshold);

    // Update engine activity indicators
    const bool active = blend > 0.01f;
    hauntPanel  .setActive (active);
    shimmerPanel.setActive (active);
    ghostPanel  .setActive (active);

    thresholdDisplay.setHauntActive      (active);
    thresholdDisplay.setShimmerActive    (active);
    thresholdDisplay.setPitchGhostActive (active);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (LiminalLookAndFeel::COBALT);

    // Title: LIMINAL
    g.setColour (LiminalLookAndFeel::GHOST_WHITE);
    g.setFont (juce::Font (18.f).withExtraKerningFactor (0.3f));
    g.drawText ("LIMINAL", 16, 10, 200, 24, juce::Justification::left);

    // Manufacturer: AMENT AUDIO
    g.setColour (LiminalLookAndFeel::GOLD_DIM);
    g.setFont (juce::Font (9.f));
    g.drawText ("AMENT AUDIO", getWidth() - 100, 14, 88, 14, juce::Justification::right);
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

    // Top row of 5 knobs
    const int knobRowH = 72;
    auto knobRow = area.removeFromTop (knobRowH);
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

    // Ramp strip at bottom
    auto rampRow = area;
    knobRampTime.setBounds (rampRow.removeFromRight (60));
    latchButton .setBounds (rampRow.removeFromRight (52).reduced (0, 16));
    rampSlider  .setBounds (rampRow.reduced (0, 14));
}
