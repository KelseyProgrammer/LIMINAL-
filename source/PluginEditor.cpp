#include "PluginEditor.h"

// The editor is a fixed 950x668 canvas: the reference artwork is drawn 1:1 as
// the background, and every interactive control is positioned exactly over its
// painted counterpart in the image. Coordinates below are pixel measurements
// taken from assets/images/liminal_bg.png.

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      knobThreshold ("threshold", "THRESHOLD", p.apvts, false),
      knobSlew      ("slew",      "SLEW",      p.apvts, false),
      knobDepth     ("depth",     "DEPTH",     p.apvts, false),
      knobTone      ("tone",      "TONE",      p.apvts, false),
      knobMix       ("mix",       "MIX",       p.apvts, false),
      hauntPanel    (EnginePanel::Engine::HauntVerb,  p.apvts),
      shimmerPanel  (EnginePanel::Engine::Shimmer,    p.apvts),
      ghostPanel    (EnginePanel::Engine::PitchGhost, p.apvts),
      knobLfoRate            ("lfoRate",          "LFO RATE",    p.apvts, false),
      knobLfoToDepth         ("lfoToDepth",       "LFO>DEPTH",   p.apvts, false),
      knobLfoToHaunt         ("lfoToHaunt",       "LFO>HAUNT",   p.apvts, false),
      knobLfoToCrystallize   ("lfoToCrystallize", "LFO>CRYST",   p.apvts, false),
      knobEnvToDepth         ("envToDepth",       "ENV>DEPTH",   p.apvts, false),
      knobRampTime           ("rampTime",         "TIME",        p.apvts, false),
      invertAttachment (p.apvts, "invertMode",  invertButton),
      latchAttachment  (p.apvts, "latch",       latchButton)
{
    setLookAndFeel (&lookAndFeel);

    backgroundImage = juce::ImageCache::getFromMemory (BinaryData::liminal_bg_png,
                                                       BinaryData::liminal_bg_pngSize);

    addAndMakeVisible (thresholdDisplay);

    // Dragging the gold ring in the display writes back to the threshold param
    if (auto* thresholdParam = p.apvts.getParameter ("threshold"))
    {
        thresholdDragAttachment = std::make_unique<juce::ParameterAttachment> (
            *thresholdParam, [] (float) {});

        thresholdDisplay.onThresholdDragStart = [this] { thresholdDragAttachment->beginGesture(); };
        thresholdDisplay.onThresholdDrag      = [this] (float v)
            { thresholdDragAttachment->setValueAsPartOfGesture (v); };
        thresholdDisplay.onThresholdDragEnd   = [this] { thresholdDragAttachment->endGesture(); };
    }

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

    setSize (950, 668);
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

    hauntPanel  .setGlowLevel (blend);
    shimmerPanel.setGlowLevel (blend);
    ghostPanel  .setGlowLevel (blend);

    thresholdDisplay.setHauntActive      (active);
    thresholdDisplay.setShimmerActive    (active);
    thresholdDisplay.setPitchGhostActive (active);

    // Feed LFO phase to look-and-feel for orbiting dot on mod knobs
    lookAndFeel.setLFOPhase (processorRef.lfoPhase.load());

    rampSlider.setValue (static_cast<double> (processorRef.rampPosition.load()),
                         juce::dontSendNotification);

    // ── Border flash when engines wake (envelope crosses below threshold) ────
    if (prevEnvelope >= threshold && envelope < threshold && borderFlash < 0.1f)
        borderFlash = 1.f;
    prevEnvelope = envelope;

    if (borderFlash > 0.f)
    {
        borderFlash = std::max (0.f, borderFlash - 0.06f);
        repaint();
    }
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff060a1c));

    if (backgroundImage.isValid())
        g.drawImage (backgroundImage, getLocalBounds().toFloat());
}

void PluginEditor::paintOverChildren (juce::Graphics& g)
{
    // Gold bloom along the filigree frame when the threshold is crossed —
    // the whole card briefly "catches the light", then settles.
    if (borderFlash > 0.01f)
    {
        const auto frame = getLocalBounds().toFloat().reduced (7.f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (borderFlash * 0.45f));
        g.drawRoundedRectangle (frame, 10.f, 3.5f);
        g.setColour (juce::Colour (0xffffe8a0).withAlpha (borderFlash * 0.30f));
        g.drawRoundedRectangle (frame.reduced (2.5f), 9.f, 1.2f);
    }
}

void PluginEditor::resized()
{
    // Central star display — centred on the painted star at (477, 130)
    thresholdDisplay.setBounds (297, 20, 360, 220);

    // Top row — painted knob centres at y=302, x = 103/278/453/629/803
    auto centred = [] (int cx, int cy, int size) {
        return juce::Rectangle<int> (cx - size / 2, cy - size / 2, size, size);
    };
    knobThreshold.setBounds (centred (103, 302, 56));
    knobSlew     .setBounds (centred (278, 302, 56));
    knobDepth    .setBounds (centred (453, 302, 56));
    knobTone     .setBounds (centred (629, 302, 56));
    knobMix      .setBounds (centred (803, 302, 56));

    invertButton.setBounds (897, 294, 32, 32);

    // Engine panels — painted panel frames with knobs at y=401
    hauntPanel  .setBounds ( 10, 355, 296, 100);
    shimmerPanel.setBounds (320, 355, 300, 100);
    ghostPanel  .setBounds (635, 355, 305, 100);

    // Modulation row — painted knob centres at y=526
    knobLfoRate          .setBounds (centred (105, 526, 44));
    knobLfoToDepth       .setBounds (centred (289, 526, 44));
    knobLfoToHaunt       .setBounds (centred (473, 526, 44));
    knobLfoToCrystallize .setBounds (centred (656, 526, 44));
    knobEnvToDepth       .setBounds (centred (841, 526, 44));

    // Ramp strip — painted track runs y≈615, A/B capsules flank it
    rampAButton.setBounds (45,  604, 26, 22);
    rampSlider .setBounds (70,  602, 702, 26);
    rampBButton.setBounds (771, 604, 28, 23);
    latchButton.setBounds (803, 602, 57, 24);
    knobRampTime.setBounds (centred (898, 604, 42));
}
