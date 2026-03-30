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

    dryBuffer.setSize (getTotalNumInputChannels(), samplesPerBlock);

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
    modMatrix.process (0.f, envelopeLevel);

    // ── 4. Ramp system ───────────────────────────────────────────────────────
    rampSystem.process (buffer.getNumSamples());

    // ── 5. Main engine process ───────────────────────────────────────────────
    liminalEngine.process (buffer, envelopeLevel);

    lastBlendLevel.store (liminalEngine.getCurrentBlend());
}

//==============================================================================
void PluginProcessor::syncParametersFromAPVTS()
{
    sThreshold  .setTargetValue (apvts.getRawParameterValue ("threshold")  ->load());
    sSlew       .setTargetValue (apvts.getRawParameterValue ("slew")       ->load());
    sDepth      .setTargetValue (apvts.getRawParameterValue ("depth")      ->load());
    sMix        .setTargetValue (apvts.getRawParameterValue ("mix")        ->load());
    sHaunt      .setTargetValue (apvts.getRawParameterValue ("haunt")      ->load());
    sCrystallize.setTargetValue (apvts.getRawParameterValue ("crystallize")->load());
    sPossession .setTargetValue (apvts.getRawParameterValue ("possession") ->load());

    liminalEngine.setThreshold   (sThreshold  .getNextValue());
    liminalEngine.setSlew        (sSlew       .getNextValue());
    liminalEngine.setDepth       (sDepth      .getNextValue());
    liminalEngine.setMix         (sMix        .getNextValue());
    liminalEngine.setHaunt       (sHaunt      .getNextValue());
    liminalEngine.setCrystallize (sCrystallize.getNextValue());
    liminalEngine.setPossession  (sPossession .getNextValue());

    const int interval = static_cast<int> (
        apvts.getRawParameterValue ("interval")->load());
    static const int semitones[] = { 12, 7, 19, 1, 6 };
    liminalEngine.setInterval (semitones[juce::jlimit (0, 4, interval)]);
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
