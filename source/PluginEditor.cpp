#include "PluginEditor.h"

// All control positions are pixel measurements against the 950x668 background
// artwork (assets/images/liminal_bg.png). The content component keeps that
// fixed coordinate space; the editor scales it to the window.

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
      invertAttachment    (p.apvts, "invertMode", invertButton),
      latchAttachment     (p.apvts, "latch",      latchButton),
      autoRampAttachment  (p.apvts, "autoRamp",   autoRampButton),
      sidechainAttachment (p.apvts, "sidechain",  sidechainButton)
{
    setLookAndFeel (&lookAndFeel);

    backgroundImage = juce::ImageCache::getFromMemory (BinaryData::liminal_bg_png,
                                                       BinaryData::liminal_bg_pngSize);

    addAndMakeVisible (content);
    content.setBounds (0, 0, kContentW, kContentH);

    content.addAndMakeVisible (thresholdDisplay);

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

    content.addAndMakeVisible (knobThreshold);
    content.addAndMakeVisible (knobSlew);
    content.addAndMakeVisible (knobDepth);
    content.addAndMakeVisible (knobTone);
    content.addAndMakeVisible (knobMix);
    content.addAndMakeVisible (invertButton);
    content.addAndMakeVisible (sidechainButton);

    content.addAndMakeVisible (hauntPanel);
    content.addAndMakeVisible (shimmerPanel);
    content.addAndMakeVisible (ghostPanel);

    content.addAndMakeVisible (knobLfoRate);
    content.addAndMakeVisible (knobLfoToDepth);
    content.addAndMakeVisible (knobLfoToHaunt);
    content.addAndMakeVisible (knobLfoToCrystallize);
    content.addAndMakeVisible (knobEnvToDepth);

    lfoSyncBox.addItemList ({ "Free", "1/1", "1/2", "1/4", "1/8", "1/16" }, 1);
    content.addAndMakeVisible (lfoSyncBox);
    lfoSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.apvts, "lfoSync", lfoSyncBox);

    rampSyncBox.addItemList ({ "Free", "1 Bar", "2 Bars", "4 Bars", "8 Bars" }, 1);
    content.addAndMakeVisible (rampSyncBox);
    rampSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.apvts, "rampSync", rampSyncBox);

    rampAButton.onClick = [this] { processorRef.captureSnapshotA(); };
    rampBButton.onClick = [this] { processorRef.captureSnapshotB(); };
    content.addAndMakeVisible (rampAButton);
    content.addAndMakeVisible (rampBButton);

    rampSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    rampSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    rampSlider.setRange (0.0, 1.0);
    rampSlider.onValueChange = [this] {
        processorRef.rampPosition.store (static_cast<float> (rampSlider.getValue()));
    };
    content.addAndMakeVisible (rampSlider);

    latchButton.onClick = [this] { processorRef.triggerRamp(); };
    content.addAndMakeVisible (latchButton);
    content.addAndMakeVisible (autoRampButton);
    content.addAndMakeVisible (knobRampTime);

    layoutContent();

    // Resizable with locked aspect ratio; restore the saved scale
    setResizable (true, true);
    if (auto* constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio (static_cast<double> (kContentW) / kContentH);
        constrainer->setSizeLimits (kContentW / 2, kContentH / 2, kContentW * 2, kContentH * 2);
    }

    const double savedScale = processorRef.apvts.state.getProperty ("uiScale", 1.0);
    setSize (juce::roundToInt (kContentW * savedScale),
             juce::roundToInt (kContentH * savedScale));

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
        content.repaint();
    }
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff060a1c));
}

void PluginEditor::resized()
{
    const float scale = static_cast<float> (getWidth()) / static_cast<float> (kContentW);
    content.setTransform (juce::AffineTransform::scale (scale));

    processorRef.apvts.state.setProperty ("uiScale", static_cast<double> (scale), nullptr);
}

void PluginEditor::paintContent (juce::Graphics& g)
{
    if (backgroundImage.isValid())
        g.drawImage (backgroundImage,
                     juce::Rectangle<float> (0.f, 0.f, static_cast<float> (kContentW),
                                             static_cast<float> (kContentH)));
}

void PluginEditor::paintContentOver (juce::Graphics& g)
{
    // Gold bloom along the filigree frame when the threshold is crossed
    if (borderFlash > 0.01f)
    {
        const auto frame = juce::Rectangle<float> (0.f, 0.f, static_cast<float> (kContentW),
                                                   static_cast<float> (kContentH)).reduced (7.f);
        g.setColour (LiminalLookAndFeel::GOLD.withAlpha (borderFlash * 0.45f));
        g.drawRoundedRectangle (frame, 10.f, 3.5f);
        g.setColour (juce::Colour (0xffffe8a0).withAlpha (borderFlash * 0.30f));
        g.drawRoundedRectangle (frame.reduced (2.5f), 9.f, 1.2f);
    }
}

void PluginEditor::layoutContent()
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

    invertButton   .setBounds (897, 294, 32, 32);
    sidechainButton.setBounds (897, 330, 32, 18);

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

    lfoSyncBox.setBounds (75, 562, 60, 17);

    // Ramp strip — painted track runs y≈615, A/B capsules flank it
    rampAButton.setBounds (45,  604, 26, 22);
    rampSlider .setBounds (70,  602, 702, 26);
    rampBButton.setBounds (771, 604, 28, 23);
    latchButton.setBounds (803, 602, 57, 24);

    rampSyncBox   .setBounds (698, 575, 64, 19);
    autoRampButton.setBounds (803, 576, 57, 18);

    knobRampTime.setBounds (centred (898, 604, 42));
}
