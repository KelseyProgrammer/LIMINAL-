#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <iterator>

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ── Core ──────────────────────────────────────────────────────────────────
    // Threshold and envelope both live in normalized dB units:
    // 0.0 = -60dB, 1.0 = 0dBFS. Default 0.5 ≈ -30dB.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "threshold", "Threshold",
        juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "slew", "Slew",
        juce::NormalisableRange<float> (5.f, 2000.f, 1.f, 0.3f), 600.f));

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

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "driftRate", "Drift Rate",
        juce::NormalisableRange<float> (0.05f, 8.f, 0.01f, 0.4f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "driftDir", "Drift Direction",
        juce::StringArray { "Down", "Wander", "Up" }, 1));

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

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "autoRamp", "Auto Ramp", false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "sidechain", "Sidechain Listen", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "rampA", "Ramp A",
        juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "rampB", "Ramp B",
        juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "rampTime", "Ramp Time",
        juce::NormalisableRange<float> (100.f, 10000.f, 1.f, 0.3f), 2000.f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "rampSync", "Ramp Sync",
        juce::StringArray { "Free", "1 Bar", "2 Bars", "4 Bars", "8 Bars" }, 0));

    // ── Modulation ────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "lfoRate", "LFO Rate",
        juce::NormalisableRange<float> (0.01f, 10.f, 0.01f, 0.4f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "lfoSync", "LFO Sync",
        juce::StringArray { "Free", "1/1", "1/2", "1/4", "1/8", "1/16" }, 0));

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
// Factory presets
//==============================================================================

namespace
{
    struct FactoryPreset
    {
        const char* name;
        float threshold, slew, depth, haunt, crystallize, possession;
        int   interval;          // 0=Oct 1=5th 2=Oct+5th 3=m2 4=Tritone
        float driftRate;
        int   driftDir;          // 0=Down 1=Wander 2=Up
        float tone, mix;
        float lfoRate, lfoToDepth, lfoToHaunt, lfoToCrystallize, envToDepth;
        bool  invertMode, autoRamp;
    };

    //                          name                  thr   slew  dep  hnt  cry  pos  int dRate dDir  tone  mix  lfoR  l>D   l>H   l>C   e>D   inv    auto
    static const FactoryPreset kPresets[] = {
        { "First Light",        0.50f,  600.f, 0.8f, 0.45f,0.00f,0.70f, 0, 0.5f, 1,  0.10f,0.80f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Last Breath",        0.55f, 1500.f, 1.0f, 0.85f,0.00f,0.80f, 0, 0.3f, 1, -0.20f,0.90f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Cathedral of Dust",  0.50f, 2000.f, 1.0f, 1.00f,0.00f,0.75f, 0, 0.3f, 1, -0.40f,1.00f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Glass Choir",        0.50f,  800.f, 0.9f, 0.70f,0.35f,0.85f, 0, 0.4f, 1,  0.15f,0.90f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Frozen Halo",        0.50f, 1000.f, 1.0f, 0.60f,1.00f,0.80f, 2, 0.3f, 1,  0.10f,0.85f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Séance",             0.55f,  900.f, 1.0f, 0.40f,0.00f,0.15f, 0, 0.3f, 1,  0.00f,1.00f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Tape Ghost",         0.50f,  700.f, 0.9f, 0.45f,0.10f,0.30f, 0, 0.8f, 0, -0.50f,0.85f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Rising Spirits",     0.50f,  900.f, 1.0f, 0.55f,0.15f,0.25f, 1, 0.6f, 2,  0.20f,0.90f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Possessed Choir",    0.55f,  500.f, 1.0f, 0.50f,0.00f,0.05f, 0, 2.0f, 1,  0.00f,1.00f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Tritone Séance",     0.50f,  900.f, 0.9f, 0.55f,0.20f,0.40f, 4, 0.4f, 1, -0.10f,0.85f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Minor 2nd Haunting", 0.50f,  800.f, 0.9f, 0.50f,0.25f,0.45f, 3, 0.5f, 1, -0.15f,0.80f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Breath Between",     0.55f,   80.f, 0.9f, 0.50f,0.00f,0.90f, 0, 0.4f, 1,  0.00f,0.85f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "The Exhale (INV)",   0.45f,  400.f, 0.8f, 0.70f,0.00f,0.70f, 0, 0.4f, 1,  0.00f,0.80f,0.5f, 0.f,  0.f,  0.f,  0.f,  true,  false },
        { "Night Tide",         0.50f, 1200.f, 0.8f, 0.60f,0.00f,0.70f, 0, 0.3f, 1, -0.10f,0.90f,0.08f,0.6f, 0.f,  0.f,  0.f,  false, false },
        { "Flickering Veil",    0.50f,  600.f, 0.9f, 0.50f,0.30f,0.70f, 0, 0.5f, 1,  0.10f,0.85f,4.0f, 0.f,  0.f,  0.5f, 0.f,  false, false },
        { "Pulse of the Void",  0.50f,  700.f, 0.9f, 0.55f,0.00f,0.65f, 0, 0.4f, 1,  0.00f,0.90f,2.0f, 0.f,  0.8f, 0.f,  0.f,  false, false },
        { "Amber Drone",        0.50f, 1000.f, 1.0f, 0.55f,1.00f,0.75f, 0, 0.3f, 1,  0.30f,1.00f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Cold Cathedral",     0.50f, 1400.f, 1.0f, 0.90f,0.00f,0.80f, 0, 0.3f, 1,  0.50f,0.95f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Velvet Dark",        0.50f,  900.f, 0.9f, 0.60f,0.00f,0.60f, 0, 0.4f, 1, -1.00f,0.70f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Whisper Gate",       0.25f, 1000.f, 1.0f, 0.80f,0.00f,0.70f, 0, 0.4f, 1,  0.00f,0.95f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Always Almost",      0.75f,  800.f, 0.6f, 0.50f,0.00f,0.70f, 0, 0.4f, 1,  0.00f,0.60f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Spectral Doubler",   0.55f,  300.f, 0.8f, 0.20f,0.00f,1.00f, 0, 0.6f, 1,  0.00f,0.50f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, false },
        { "Stairs to Nowhere",  0.50f,  900.f, 1.0f, 0.65f,0.50f,0.70f, 2, 0.4f, 1,  0.05f,0.90f,0.3f, 0.f,  0.f, -0.3f, 0.f,  false, false },
        { "Self-Morphing",      0.50f,  800.f, 1.0f, 0.60f,0.20f,0.50f, 0, 0.5f, 1,  0.00f,0.90f,0.5f, 0.f,  0.f,  0.f,  0.f,  false, true  },
    };

    constexpr int kNumPresets = static_cast<int> (std::size (kPresets));
}

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                       .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                       .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "LIMINAL_STATE", createParameterLayout())
{
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
const juce::String PluginProcessor::getName() const { return JucePlugin_Name; }

bool PluginProcessor::acceptsMidi()  const { return true; }   // note-ons trigger ghost captures
bool PluginProcessor::producesMidi() const { return false; }
bool PluginProcessor::isMidiEffect() const { return false; }

double PluginProcessor::getTailLengthSeconds() const
{
    return 10.0;  // HauntVerb tank at full HAUNT rings out for many seconds
}

//==============================================================================
int PluginProcessor::getNumPrograms()    { return kNumPresets; }
int PluginProcessor::getCurrentProgram() { return currentProgram; }

const juce::String PluginProcessor::getProgramName (int index)
{
    if (index >= 0 && index < kNumPresets)
        return kPresets[index].name;
    return {};
}

void PluginProcessor::changeProgramName (int, const juce::String&) {}

void PluginProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= kNumPresets)
        return;

    currentProgram = index;
    const auto& p = kPresets[index];

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    set ("threshold",        p.threshold);
    set ("slew",             p.slew);
    set ("depth",            p.depth);
    set ("haunt",            p.haunt);
    set ("crystallize",      p.crystallize);
    set ("possession",       p.possession);
    set ("interval",         static_cast<float> (p.interval));
    set ("driftRate",        p.driftRate);
    set ("driftDir",         static_cast<float> (p.driftDir));
    set ("tone",             p.tone);
    set ("mix",              p.mix);
    set ("lfoRate",          p.lfoRate);
    set ("lfoToDepth",       p.lfoToDepth);
    set ("lfoToHaunt",       p.lfoToHaunt);
    set ("lfoToCrystallize", p.lfoToCrystallize);
    set ("envToDepth",       p.envToDepth);
    set ("invertMode",       p.invertMode ? 1.f : 0.f);
    set ("autoRamp",         p.autoRamp   ? 1.f : 0.f);
}

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
    sThreshold  .reset (sr, 0.05); sThreshold  .setCurrentAndTargetValue (0.5f);
    sSlew       .reset (sr, 0.05); sSlew       .setCurrentAndTargetValue (600.f);
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
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono()
     && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainOut != layouts.getMainInputChannelSet())
        return false;

    // Sidechain (input bus 1): disabled, mono or stereo all fine
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled()
         && sc != juce::AudioChannelSet::mono()
         && sc != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

//==============================================================================
float PluginProcessor::detectEnvelope (juce::AudioBuffer<float>& buffer)
{
    // Runs the follower over a buffer view (no copies — RT safe)
    auto runDetection = [this] (const juce::AudioBuffer<float>& detect) -> float
    {
        const int numSamples  = detect.getNumSamples();
        const int numChannels = detect.getNumChannels();

        float level = 0.f;
        for (int s = 0; s < numSamples; ++s)
        {
            // Mono detection: max |x| across channels (robust to hard panning)
            float peak = 0.f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = juce::jmax (peak, std::abs (detect.getReadPointer (ch)[s]));

            level = envelopeFollower.process (peak);
        }
        return level;
    };

    float level;
    const bool wantSidechain = apvts.getRawParameterValue ("sidechain")->load() > 0.5f;

    if (wantSidechain && getBusCount (true) > 1
        && getBusBuffer (buffer, true, 1).getNumChannels() > 0)
    {
        level = runDetection (getBusBuffer (buffer, true, 1));
    }
    else
    {
        level = runDetection (getBusBuffer (buffer, true, 0));
    }

    // Map to normalized dB units: -60dB → 0.0, 0dBFS → 1.0
    const float db = juce::Decibels::gainToDecibels (level, -72.f);
    return juce::jlimit (0.f, 1.f, (db + 60.f) / 60.f);
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    // ── MIDI: any note-on conjures a ghost from the current ring buffer ──────
    for (const auto metadata : midiMessages)
        if (metadata.getMessage().isNoteOn())
            liminalEngine.triggerGhostCapture();

    // ── Host tempo (fallback 120 when not provided) ───────────────────────────
    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto hostBpm = position->getBpm())
                if (*hostBpm > 0.0)
                    bpm = *hostBpm;

    // ── 1. Update parameters from APVTS ──────────────────────────────────────
    syncParametersFromAPVTS (numSamples, bpm);

    // ── 2. Envelope detection (main input or sidechain, mono max, dB units) ──
    const float envelopeLevel = detectEnvelope (buffer);
    lastEnvelopeLevel.store (envelopeLevel);

    // ── 3. Update modulation ─────────────────────────────────────────────────
    modMatrix.process (envelopeLevel);

    // LFO phase accumulation for UI visualization (0.0–1.0) — mirrors the
    // effective (possibly tempo-synced) rate set in syncParametersFromAPVTS
    {
        const float rate = modMatrix.getCurrentLFORate();
        float phase = lfoPhase.load();
        phase += rate * static_cast<float> (numSamples) / static_cast<float> (getSampleRate());
        if (phase >= 1.f) phase -= std::floor (phase);
        lfoPhase.store (phase);
    }

    // ── 4. Ramp system ───────────────────────────────────────────────────────
    const bool latchOn    = apvts.getRawParameterValue ("latch")->load() > 0.5f;
    const bool autoRampOn = apvts.getRawParameterValue ("autoRamp")->load() > 0.5f;

    if (latchOn || autoRampOn)
    {
        if (rampTriggerPending.exchange (false))
            rampSystem.trigger();

        float rampTimeMs = apvts.getRawParameterValue ("rampTime")->load();
        const int rampSyncIdx = static_cast<int> (apvts.getRawParameterValue ("rampSync")->load());
        if (rampSyncIdx > 0)
        {
            static const float bars[] = { 0.f, 1.f, 2.f, 4.f, 8.f };
            rampTimeMs = static_cast<float> (bars[rampSyncIdx] * 4.0 * 60000.0 / bpm);
        }

        rampSystem.setRampTime (rampTimeMs, getSampleRate());
        rampSystem.process (numSamples);
        rampPosition.store (rampSystem.getPosition());
    }

    // ── 5. Main engine process ───────────────────────────────────────────────
    liminalEngine.process (buffer, envelopeLevel);

    const float newBlend = liminalEngine.getCurrentBlend();
    lastBlendLevel.store (newBlend);

    // ── 6. Auto-ramp: trigger the morph as the engines wake ──────────────────
    if (autoRampOn
        && prevBlendForAutoRamp < 0.05f && newBlend >= 0.05f)
    {
        rampTriggerPending.store (true);
    }
    prevBlendForAutoRamp = newBlend;
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
void PluginProcessor::syncParametersFromAPVTS (int numSamples, double bpm)
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
        // Advance the 50ms smoothing ramps by a full block per block —
        // skip() is the per-block API; getNextValue() once per block made the
        // ramps run hundreds of times slower than designed.
        threshold   = sThreshold  .skip (numSamples);
        slew        = sSlew       .skip (numSamples);
        depth       = sDepth      .skip (numSamples);
        haunt       = sHaunt      .skip (numSamples);
        crystallize = sCrystallize.skip (numSamples);
        possession  = sPossession .skip (numSamples);
        mix         = sMix        .skip (numSamples);
        const int idx = static_cast<int> (apvts.getRawParameterValue ("interval")->load());
        static const int semitones[] = { 12, 7, 19, 1, 6 };
        interval = semitones[juce::jlimit (0, 4, idx)];
    }

    // ── LFO rate, free or tempo-synced ────────────────────────────────────────
    float lfoRateHz = apvts.getRawParameterValue ("lfoRate")->load();
    {
        const int lfoSyncIdx = static_cast<int> (apvts.getRawParameterValue ("lfoSync")->load());
        if (lfoSyncIdx > 0)
        {
            // Beats per LFO cycle: 1/1 = 4 beats … 1/16 = 1/4 beat
            static const float beatsPerCycle[] = { 0.f, 4.f, 2.f, 1.f, 0.5f, 0.25f };
            lfoRateHz = static_cast<float> ((bpm / 60.0) / beatsPerCycle[lfoSyncIdx]);
        }
    }

    // ── ModMatrix routing + apply (one block latency — fine for LFO rates) ────
    modMatrix.setLFORate (lfoRateHz);
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

    liminalEngine.setDriftRate (apvts.getRawParameterValue ("driftRate")->load());
    liminalEngine.setDriftDirection (
        static_cast<int> (apvts.getRawParameterValue ("driftDir")->load()) - 1);

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
