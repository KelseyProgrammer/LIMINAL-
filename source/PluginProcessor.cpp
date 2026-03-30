#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ── Core ──────────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "threshold", "Threshold",
        juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 0.3f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "slew", "Slew",
        juce::NormalisableRange<float> (5.f, 2000.f, 1.f, 0.3f), 200.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "depth", "Depth",
        juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 1.0f));

    // ── Engines ───────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "haunt", "Haunt",
        juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "crystallize", "Crystallize",
        juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "possession", "Possession",
        juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "interval", "Interval",
        juce::StringArray { "Oct", "5th", "Oct+5th", "m2", "Tritone" }, 0));

    // ── Tone / Output ─────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "tone", "Tone",
        juce::NormalisableRange<float> (-1.f, 1.f, 0.01f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "mix", "Mix",
        juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 1.0f));

    // ── Modulation / Advanced ─────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterBool> (
        "invertMode", "Invert Mode", false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "latch", "Latch", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "rampA", "Ramp A",
        juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "rampB", "Ramp B",
        juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "rampTime", "Ramp Time",
        juce::NormalisableRange<float> (100.f, 10000.f, 1.f, 0.3f), 2000.f));

    // ── Modulation ────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "lfoRate", "LFO Rate",
        juce::NormalisableRange<float> (0.01f, 10.f, 0.01f, 0.4f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "lfoToDepth", "LFO → Depth",
        juce::NormalisableRange<float> (-1.f, 1.f, 0.01f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "lfoToHaunt", "LFO → Haunt",
        juce::NormalisableRange<float> (-1.f, 1.f, 0.01f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "lfoToCrystallize", "LFO → Crystallize",
        juce::NormalisableRange<float> (-1.f, 1.f, 0.01f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "envToDepth", "Env → Depth",
        juce::NormalisableRange<float> (-1.f, 1.f, 0.01f), 0.f));

    return layout;
}

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LIMINAL_STATE", createParameterLayout())
{
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
const juce::String PluginProcessor::getName() const { return JucePlugin_Name; }

bool PluginProcessor::acceptsMidi()  const { return false; }
bool PluginProcessor::producesMidi() const { return false; }
bool PluginProcessor::isMidiEffect() const { return false; }

double PluginProcessor::getTailLengthSeconds() const
{
    return 2.0;  // Up to 2s of reverb/ghost tail
}

int               PluginProcessor::getNumPrograms()              { return 1; }
int               PluginProcessor::getCurrentProgram()           { return 0; }
void              PluginProcessor::setCurrentProgram (int)       {}
const juce::String PluginProcessor::getProgramName (int)         { return {}; }
void              PluginProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32> (samplesPerBlock),
        static_cast<juce::uint32> (getTotalNumOutputChannels())
    };

    envelopeFollower.prepare (sampleRate);
    liminalEngine.prepare    (spec);
    modMatrix.prepare        (spec);
    rampSystem.setRampTime   (2000.f, sampleRate);

    // Initialise smoothed values (50ms ramp)
    const double sr = sampleRate;
    sThreshold  .reset (sr, 0.05); sThreshold  .setCurrentAndTargetValue (0.3f);
    sSlew       .reset (sr, 0.05); sSlew       .setCurrentAndTargetValue (200.f);
    sDepth      .reset (sr, 0.05); sDepth      .setCurrentAndTargetValue (1.f);
    sMix        .reset (sr, 0.05); sMix        .setCurrentAndTargetValue (1.f);
    sHaunt      .reset (sr, 0.05); sHaunt      .setCurrentAndTargetValue (0.5f);
    sCrystallize.reset (sr, 0.05); sCrystallize.setCurrentAndTargetValue (0.f);
    sPossession .reset (sr, 0.05); sPossession .setCurrentAndTargetValue (0.5f);
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

//==============================================================================
void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    // ── 1. Update parameters from APVTS ──────────────────────────────────────
    syncParametersFromAPVTS();

    // ── 2. Process envelope follower (per sample, channel 0) ─────────────────
    float envelopeLevel = 0.f;
    for (int s = 0; s < buffer.getNumSamples(); ++s)
        envelopeLevel = envelopeFollower.process (buffer.getSample (0, s));

    lastEnvelopeLevel.store (envelopeLevel);

    // ── 3. Update modulation ─────────────────────────────────────────────────
    modMatrix.process (envelopeLevel);

    // LFO phase accumulation for UI visualization (0.0–1.0)
    {
        const float rate = apvts.getRawParameterValue ("lfoRate")->load();
        float phase = lfoPhase.load();
        phase += rate * static_cast<float> (buffer.getNumSamples()) / static_cast<float> (getSampleRate());
        if (phase >= 1.f) phase -= std::floor (phase);
        lfoPhase.store (phase);
    }

    // ── 4. Ramp system ───────────────────────────────────────────────────────
    const bool latchOn = apvts.getRawParameterValue ("latch")->load() > 0.5f;
    if (latchOn)
    {
        if (rampTriggerPending.exchange (false))
            rampSystem.trigger();

        const float rampTimeMs = apvts.getRawParameterValue ("rampTime")->load();
        rampSystem.setRampTime (rampTimeMs, getSampleRate());
        rampSystem.process (buffer.getNumSamples());
        rampPosition.store (rampSystem.getPosition());
    }

    // ── 5. Main engine process ───────────────────────────────────────────────
    liminalEngine.process (buffer, envelopeLevel);

    lastBlendLevel.store (liminalEngine.getCurrentBlend());
}

//==============================================================================
void PluginProcessor::captureSnapshotA()
{
    snapshotA.threshold   = apvts.getRawParameterValue ("threshold")  ->load();
    snapshotA.slew        = apvts.getRawParameterValue ("slew")       ->load();
    snapshotA.depth       = apvts.getRawParameterValue ("depth")      ->load();
    snapshotA.haunt       = apvts.getRawParameterValue ("haunt")      ->load();
    snapshotA.crystallize = apvts.getRawParameterValue ("crystallize")->load();
    snapshotA.possession  = apvts.getRawParameterValue ("possession") ->load();
    snapshotA.mix         = apvts.getRawParameterValue ("mix")        ->load();
    snapshotA.interval    = static_cast<int> (apvts.getRawParameterValue ("interval")->load());
    hasSnapshotA = true;
}

void PluginProcessor::captureSnapshotB()
{
    snapshotB.threshold   = apvts.getRawParameterValue ("threshold")  ->load();
    snapshotB.slew        = apvts.getRawParameterValue ("slew")       ->load();
    snapshotB.depth       = apvts.getRawParameterValue ("depth")      ->load();
    snapshotB.haunt       = apvts.getRawParameterValue ("haunt")      ->load();
    snapshotB.crystallize = apvts.getRawParameterValue ("crystallize")->load();
    snapshotB.possession  = apvts.getRawParameterValue ("possession") ->load();
    snapshotB.mix         = apvts.getRawParameterValue ("mix")        ->load();
    snapshotB.interval    = static_cast<int> (apvts.getRawParameterValue ("interval")->load());
    hasSnapshotB = true;
}

//==============================================================================
void PluginProcessor::syncParametersFromAPVTS()
{
    // Always keep smoothed values tracking APVTS (used when no snapshots)
    sThreshold  .setTargetValue (apvts.getRawParameterValue ("threshold")  ->load());
    sSlew       .setTargetValue (apvts.getRawParameterValue ("slew")       ->load());
    sDepth      .setTargetValue (apvts.getRawParameterValue ("depth")      ->load());
    sMix        .setTargetValue (apvts.getRawParameterValue ("mix")        ->load());
    sHaunt      .setTargetValue (apvts.getRawParameterValue ("haunt")      ->load());
    sCrystallize.setTargetValue (apvts.getRawParameterValue ("crystallize")->load());
    sPossession .setTargetValue (apvts.getRawParameterValue ("possession") ->load());

    float threshold, slew, depth, haunt, crystallize, possession, mix;
    int   interval;

    if (hasSnapshotA && hasSnapshotB)
    {
        const float t = rampPosition.load();
        auto lerp = [] (float a, float b, float p) { return a + (b - a) * p; };

        threshold   = lerp (snapshotA.threshold,   snapshotB.threshold,   t);
        slew        = lerp (snapshotA.slew,        snapshotB.slew,        t);
        depth       = lerp (snapshotA.depth,       snapshotB.depth,       t);
        haunt       = lerp (snapshotA.haunt,       snapshotB.haunt,       t);
        crystallize = lerp (snapshotA.crystallize, snapshotB.crystallize, t);
        possession  = lerp (snapshotA.possession,  snapshotB.possession,  t);
        mix         = lerp (snapshotA.mix,         snapshotB.mix,         t);
        interval    = (t < 0.5f) ? snapshotA.interval : snapshotB.interval;

        // Advance smoothed values so they don't jump when ramp is deactivated
        sThreshold  .setCurrentAndTargetValue (threshold);
        sSlew       .setCurrentAndTargetValue (slew);
        sDepth      .setCurrentAndTargetValue (depth);
        sMix        .setCurrentAndTargetValue (mix);
        sHaunt      .setCurrentAndTargetValue (haunt);
        sCrystallize.setCurrentAndTargetValue (crystallize);
        sPossession .setCurrentAndTargetValue (possession);
    }
    else
    {
        threshold   = sThreshold  .getNextValue();
        slew        = sSlew       .getNextValue();
        depth       = sDepth      .getNextValue();
        haunt       = sHaunt      .getNextValue();
        crystallize = sCrystallize.getNextValue();
        possession  = sPossession .getNextValue();
        mix         = sMix        .getNextValue();
        const int idx = static_cast<int> (apvts.getRawParameterValue ("interval")->load());
        static const int semitones[] = { 12, 7, 19, 1, 6 };
        interval = semitones[juce::jlimit (0, 4, idx)];
    }

    // ── ModMatrix routing + apply (one block latency — fine for LFO rates) ──────
    modMatrix.setLFORate (apvts.getRawParameterValue ("lfoRate")->load());
    modMatrix.setRouting (ModMatrix::LFO,      ModMatrix::DEPTH,       apvts.getRawParameterValue ("lfoToDepth")      ->load());
    modMatrix.setRouting (ModMatrix::LFO,      ModMatrix::HAUNT,       apvts.getRawParameterValue ("lfoToHaunt")      ->load());
    modMatrix.setRouting (ModMatrix::LFO,      ModMatrix::CRYSTALLIZE, apvts.getRawParameterValue ("lfoToCrystallize")->load());
    modMatrix.setRouting (ModMatrix::ENVELOPE, ModMatrix::DEPTH,       apvts.getRawParameterValue ("envToDepth")      ->load());

    depth       = juce::jlimit (0.f, 1.f, depth       + modMatrix.getModValue (ModMatrix::DEPTH));
    haunt       = juce::jlimit (0.f, 1.f, haunt       + modMatrix.getModValue (ModMatrix::HAUNT));
    crystallize = juce::jlimit (0.f, 1.f, crystallize + modMatrix.getModValue (ModMatrix::CRYSTALLIZE));

    liminalEngine.setThreshold   (threshold);
    liminalEngine.setSlew        (slew);
    liminalEngine.setDepth       (depth);
    liminalEngine.setMix         (mix);
    liminalEngine.setHaunt       (haunt);
    liminalEngine.setCrystallize (crystallize);
    liminalEngine.setPossession  (possession);
    liminalEngine.setInterval    (interval);

    const float toneVal    = apvts.getRawParameterValue ("tone")      ->load();
    const bool  invertMode = apvts.getRawParameterValue ("invertMode")->load() > 0.5f;
    liminalEngine.setTone       (toneVal);
    liminalEngine.setInvertMode (invertMode);
}

//==============================================================================
bool PluginProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
