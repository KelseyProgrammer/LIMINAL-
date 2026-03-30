# LIMINAL — Claude.md
## Ament Audio | VST3/AU Plugin | JUCE + Pamplejuce

---

## Project Overview

**LIMINAL** is a threshold-based negative-space effect plugin. It monitors the input signal's amplitude envelope and activates a suite of atmospheric engines — HAUNT VERB, SHIMMER, and PITCH GHOST — in the space *between* notes: decays, silences, and breaths. The louder the silence, the more alive LIMINAL becomes.

**Tagline:** *"The effect that wakes up when you stop playing."*

**Format:** VST3 / AU (macOS + Windows)
**Template:** [Pamplejuce](https://github.com/sudara/pamplejuce) (JUCE 7.x, CMake)
**Plugin Type:** `AudioProcessor` — effect (not instrument)
**Stereo I/O:** Yes. Mono-compatible.

---

## Brand & Aesthetic Context

LIMINAL is the third plugin in the Ament Audio trilogy:

| Plugin | Domain | Transforms |
|---|---|---|
| FREECODER | Spectral content | Morphs timbres |
| HALATION | The bloom/halo | Diffuses and glows |
| LIMINAL | Negative space | Haunts the silence |

UI color language: **deep cobalt (`#0a0f2e`), aged gold (`#c9a84c`), ice blue (`#a8c4e8`), ghost white (`#e8e8f0`)** — inspired by celestial tarot geometry. The mysticism comes through geometry and glow, not ornamentation. See UI section for full spec.

---

## Repository Structure (Pamplejuce)

```
LIMINAL/
├── CMakeLists.txt
├── CMakePresets.json
├── Claude.md                        ← this file
├── SETUP_GUIDE.md
├── .github/
│   └── workflows/
│       └── cmake_ctest.yml
├── source/
│   ├── PluginProcessor.h / .cpp     ← AudioProcessor root
│   ├── PluginEditor.h / .cpp        ← AudioProcessorEditor root
│   │
│   ├── dsp/
│   │   ├── EnvelopeFollower.h / .cpp
│   │   ├── LiminalEngine.h / .cpp   ← master DSP coordinator
│   │   ├── HauntVerb.h / .cpp
│   │   ├── Shimmer.h / .cpp
│   │   └── PitchGhost.h / .cpp
│   │
│   ├── modulation/
│   │   ├── RampSystem.h / .cpp      ← RAMP A/B morph system
│   │   └── ModMatrix.h / .cpp       ← LFO + envelope routing
│   │
│   └── ui/
│       ├── LiminalLookAndFeel.h / .cpp
│       ├── ThresholdDisplay.h / .cpp  ← waveform + threshold line
│       ├── EnginePanel.h / .cpp       ← three engine sub-panels
│       └── KnobComponent.h / .cpp
│
├── tests/
│   └── LiminalTests.cpp
└── assets/
    └── fonts/
```

---

## Class Architecture

### Top Level

```
PluginProcessor (AudioProcessor)
    │
    ├── AudioProcessorValueTreeState (APVTS)
    ├── EnvelopeFollower
    ├── LiminalEngine
    │     ├── HauntVerb
    │     ├── Shimmer
    │     └── PitchGhost
    ├── RampSystem
    └── ModMatrix

PluginEditor (AudioProcessorEditor)
    │
    ├── LiminalLookAndFeel
    ├── ThresholdDisplay
    ├── EnginePanel (x3 sub-panels)
    └── KnobComponent (x9 parameter knobs)
```

---

## DSP Architecture

### EnvelopeFollower

Tracks the RMS amplitude of the input signal and outputs a normalized 0.0–1.0 value.

```cpp
class EnvelopeFollower {
public:
    void prepare(double sampleRate);
    float process(float inputSample);       // call per sample
    float getCurrentLevel() const;          // 0.0 - 1.0
    void setAttack(float attackMs);
    void setRelease(float releaseMs);

private:
    float envelope = 0.f;
    float attackCoeff, releaseCoeff;
    double sampleRate;
};
```

- **Attack:** ~5ms (fast enough to catch transients)
- **Release:** ~200ms default (user-adjustable via SLEW parameter)
- Uses standard one-pole IIR envelope follower formula

---

### LiminalEngine

Master DSP coordinator. Receives the envelope level, compares to threshold, computes a wet blend factor, and routes audio through the three engines.

```cpp
class LiminalEngine {
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::AudioBuffer<float>& buffer, float envelopeLevel);
    void setThreshold(float normalizedThreshold);   // 0.0 - 1.0
    void setSlew(float slewMs);
    void setDepth(float depth);                     // 0.0 - 1.0
    void setMix(float mix);                         // 0.0 - 1.0

private:
    float computeBlend(float envelopeLevel);        // returns 0.0-1.0 wet factor
    float currentBlend = 0.f;
    float targetBlend  = 0.f;
    float slewRate     = 0.f;

    HauntVerb  hauntVerb;
    Shimmer    shimmer;
    PitchGhost pitchGhost;

    float threshold = 0.3f;
    float depth     = 1.0f;
    float mix       = 1.0f;
};
```

**Blend logic:**
```
if envelopeLevel < threshold:
    targetBlend = (1.0 - (envelopeLevel / threshold)) * depth
else:
    targetBlend = 0.0

currentBlend += (targetBlend - currentBlend) * slewRate  // one-pole slew
```

---

### HauntVerb

A diffusion-based reverb whose character *opens up* as the signal fades. Not a standard convolution reverb — uses an allpass diffusion network with a high-pass filter that sweeps upward as signal drops.

```cpp
class HauntVerb {
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::AudioBuffer<float>& buffer, float blendFactor);
    void setHaunt(float amount);        // 0.0 - 1.0, bloom intensity
    void setEnvelopeLevel(float level); // fed from EnvelopeFollower

private:
    // Allpass diffusion network (4 stages per channel)
    juce::dsp::DelayLine<float> diffusionLines[4];
    float diffusionCoeffs[4] = { 0.7f, 0.6f, 0.5f, 0.4f };

    // High-pass that opens as level drops
    juce::dsp::StateVariableTPTFilter<float> hpFilter;
    float computeHPFrequency(float envelopeLevel); // maps 0.0→4kHz, 1.0→80Hz

    float hauntAmount = 0.5f;
    double sampleRate = 44100.0;
};
```

**Key behavior:**
- Pre-delay stretches from 10ms → 60ms as envelope drops
- Diffusion increases (longer delay times) as signal fades
- HP filter sweeps from ~80Hz at full signal to ~4kHz in deep silence — counterintuitive brightening that creates the "exhale" quality

---

### Shimmer

Pitch-shifted feedback layer. Uses two pitch shifter voices (primary interval + optional second interval) that feed back into themselves. Activates only in the negative space via blend factor from LiminalEngine.

```cpp
class Shimmer {
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::AudioBuffer<float>& buffer, float blendFactor);
    void setCrystallize(float amount);      // 0.0 - 1.0, freeze/hold
    void setInterval(int semitones);        // interval in semitones
    void setFeedback(float feedback);       // 0.0 - 0.95

private:
    // Phase vocoder pitch shifter (two voices)
    // Voice 1: user-defined interval
    // Voice 2: octave above voice 1
    PitchShifterVoice voice1, voice2;

    float crystallizeAmount = 0.f;
    float feedbackLevel     = 0.5f;
    int   intervalSemitones = 12;           // default: octave up

    juce::AudioBuffer<float> feedbackBuffer;
};
```

**Crystallize behavior:**
- At 0.0: shimmer decays normally with the blend factor
- At 1.0: shimmer output is latched and held as a sustained drone regardless of further signal changes. Acts like a freeze.
- Intermediate values: partial hold with slow decay

**Interval presets (map to UI selector):**
```
0: Octave Up (+12)
1: Fifth Up (+7)
2: Octave + Fifth (+19)
3: Minor 2nd (+1) — dissonant/eerie
4: Tritone (+6)   — maximally unsettling
```

---

### PitchGhost

The most unique engine. Captures a micro-snapshot (64–256 samples) of the input signal just as it crosses below the threshold, then plays it back as a detuned phantom voice that slowly drifts away from the original pitch.

```cpp
class PitchGhost {
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::AudioBuffer<float>& buffer, float blendFactor);
    void setPossession(float amount);       // 0.0 - 1.0, drift intensity
    void setDriftRate(float rateHz);        // how fast ghost detunes
    void setDriftDirection(int direction);  // -1=down, 0=wander, 1=up
    void setGhostDecay(float decayMs);

    // Called by LiminalEngine when threshold is crossed downward
    void triggerCapture(const juce::AudioBuffer<float>& inputBuffer);

private:
    juce::AudioBuffer<float> captureBuffer;
    bool   isCapturing  = false;
    bool   ghostActive  = false;
    float  currentPitch = 0.f;      // current detuning in cents
    float  targetPitch  = 0.f;
    float  possession   = 0.5f;
    float  driftRate    = 0.1f;     // Hz
    int    driftDir     = 0;

    float  decayEnvelope = 1.f;
    float  decayCoeff    = 0.f;

    void updateDrift(float deltaTime);
};
```

**Possession behavior:**
- **Low possession (0.0–0.3):** Ghost immediately drifts far and free — sounds alien
- **Mid possession (0.3–0.7):** Ghost struggles between source pitch and drift — creates beating/chorus
- **High possession (0.8–1.0):** Ghost barely drifts, stays close to source pitch — tight shimmer double

---

## Parameter Model (APVTS)

All parameters registered in `PluginProcessor.cpp` via `AudioProcessorValueTreeState`.

```cpp
// In createParameterLayout():

layout.add(std::make_unique<juce::AudioParameterFloat>(
    "threshold", "Threshold",
    juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.3f));

layout.add(std::make_unique<juce::AudioParameterFloat>(
    "slew", "Slew",
    juce::NormalisableRange<float>(5.f, 2000.f, 1.f, 0.3f), 200.f)); // ms, skewed

layout.add(std::make_unique<juce::AudioParameterFloat>(
    "depth", "Depth",
    juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 1.0f));

layout.add(std::make_unique<juce::AudioParameterFloat>(
    "haunt", "Haunt",
    juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.5f));

layout.add(std::make_unique<juce::AudioParameterFloat>(
    "crystallize", "Crystallize",
    juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.0f));

layout.add(std::make_unique<juce::AudioParameterFloat>(
    "possession", "Possession",
    juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.5f));

layout.add(std::make_unique<juce::AudioParameterChoice>(
    "interval", "Interval",
    juce::StringArray{"Oct", "5th", "Oct+5th", "m2", "Tritone"}, 0));

layout.add(std::make_unique<juce::AudioParameterFloat>(
    "tone", "Tone",
    juce::NormalisableRange<float>(-1.f, 1.f, 0.01f), 0.0f)); // -1=dark, +1=bright

layout.add(std::make_unique<juce::AudioParameterFloat>(
    "mix", "Mix",
    juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 1.0f));

// Modulation / advanced
layout.add(std::make_unique<juce::AudioParameterBool>(
    "invertMode", "Invert Mode", false));

layout.add(std::make_unique<juce::AudioParameterBool>(
    "latch", "Latch", false));

layout.add(std::make_unique<juce::AudioParameterFloat>(
    "rampA", "Ramp A",
    juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.0f));

layout.add(std::make_unique<juce::AudioParameterFloat>(
    "rampB", "Ramp B",
    juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 1.0f));

layout.add(std::make_unique<juce::AudioParameterFloat>(
    "rampTime", "Ramp Time",
    juce::NormalisableRange<float>(100.f, 10000.f, 1.f, 0.3f), 2000.f)); // ms
```

---

## Modulation System

### RampSystem

Interpolates between two parameter states (RAMP A and RAMP B) over a user-defined time. Chase Bliss-style state morphing.

```cpp
class RampSystem {
public:
    void setRampTime(float timeMs, double sampleRate);
    void trigger();                         // start morph A→B or B→A
    float getPosition() const;             // 0.0 = state A, 1.0 = state B
    void process(int numSamples);

private:
    float position     = 0.f;
    float increment    = 0.f;
    bool  direction    = true;             // true = A→B, false = B→A
};
```

### ModMatrix

Routes LFO and envelope sources to parameter destinations.

```cpp
class ModMatrix {
public:
    enum Source  { LFO, ENVELOPE };
    enum Dest    { THRESHOLD, DEPTH, HAUNT, CRYSTALLIZE, POSSESSION, TONE };

    void setRouting(Source src, Dest dest, float amount); // amount: -1.0 to 1.0
    float getModValue(Dest dest) const;
    void process(float lfoValue, float envelopeValue);

private:
    struct Route { Source src; Dest dest; float amount; };
    std::vector<Route> routes;
    float modValues[6] = {};
};
```

---

## Audio Processing Flow (processBlock)

```cpp
void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // 1. Update parameters from APVTS
    syncParametersFromAPVTS();

    // 2. Process envelope follower (per sample, use first channel)
    float envelopeLevel = 0.f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        envelopeLevel = envelopeFollower.process(buffer.getSample(0, sample));
    }

    // 3. Update modulation
    float lfoValue = modMatrix.getLFOValue(); // internal LFO tick
    modMatrix.process(lfoValue, envelopeLevel);

    // 4. Apply ramp system
    rampSystem.process(buffer.getNumSamples());

    // 5. Main engine process
    liminalEngine.process(buffer, envelopeLevel);

    // 6. Apply output mix (wet/dry)
    applyMix(buffer, dryBuffer);
}
```

---

## UI Architecture

### Design Philosophy

The UI draws from **celestial tarot geometry** — deep cobalt blue fields, aged gold line work, and a central radial/star motif — filtered through a clean, functional plugin aesthetic. The mysticism comes from geometry and glow, not ornamentation. No filigree. No decorative borders. The star breathes; everything else is disciplined.

Reference image: tarot card with deep cobalt background, gold hexagram with radiating lines, moon/star corner medallions, and a luminous gradient from dark center outward.

---

### Layout (800 × 500px)

```
┌─────────────────────────────────────────────────────────┐
│  LIMINAL                             [AMENT AUDIO logo] │
│  ─────────────────────────────────────────────────────  │
│                                                         │
│        ┌──────────────────────────────────┐             │
│        │     CELESTIAL THRESHOLD DISPLAY  │             │
│        │                                  │             │
│        │   ✦ hexagram — breathes with     │             │
│        │     engine activation level      │             │
│        │   — gold threshold ring          │             │
│        │   — radiating lines pulse        │             │
│        │     per engine (3 axes)          │             │
│        └──────────────────────────────────┘             │
│                                                         │
│  [THRESHOLD]  [SLEW]   [DEPTH]   [TONE]   [MIX]        │
│                                                         │
│  ┌──────────────┬──────────────┬──────────────────┐     │
│  │  HAUNT VERB  │   SHIMMER    │   PITCH GHOST    │     │
│  │              │              │                  │     │
│  │  [HAUNT]     │ [CRYSTALLIZE]│  [POSSESSION]    │     │
│  │              │ [INTERVAL ▾] │  [DRIFT DIR ◀▶]  │     │
│  │  ☽ active    │  ✦ active    │   ✧ active       │     │
│  └──────────────┴──────────────┴──────────────────┘     │
│                                                         │
│  [RAMP A] ────────●──── [RAMP B]  [RAMP TIME] [LATCH]  │
└─────────────────────────────────────────────────────────┘
```

---

### CelestialThresholdDisplay

The central display replaces a scrolling waveform with a **living geometric star**. This is the visual heart of LIMINAL.

**Structure:**
- A six-pointed star (hexagram) drawn with gold line work (`#c9a84c`) at center
- The star has six radiating lines extending outward — think the tarot card's central geometry
- The overall star **scales up smoothly** as the Liminal Engine blend increases (engines waking in the silence)
- When blend = 0 (above threshold, dry): star is small, dim, barely visible
- When blend = 1 (deep silence): star is fully expanded, gold lines bright, radiating glow

**Per-engine axis pulses:**
- The six star points are grouped in pairs — one pair per engine
- HAUNT VERB axes pulse in **ice blue** (`#a8c4e8`) when active
- SHIMMER axes pulse in **aged gold** (`#c9a84c`) when active
- PITCH GHOST axes pulse in **ghost white** (`#e8e8f0`) when active

**Threshold ring:**
- A circular ring sits at a fixed radius from center
- Ring radius maps to the THRESHOLD parameter value — draggable
- The envelope level is shown as a second ring that moves inward/outward in real time
- When the envelope ring crosses inside the threshold ring: engines activate

**Background gradient:**
- Radial gradient from `#1a2050` (center, slightly lighter cobalt) outward to `#0a0f2e` (edge)
- Mirrors the luminous-center-to-dark-edge quality of the reference image

**Implementation notes:**
- Draw using `juce::Graphics::drawLine`, `juce::Path`, and `juce::ColourGradient`
- Star geometry computed from center point with 6 vertices at equal angular intervals (60° apart)
- Scale factor driven by `currentBlend` value from LiminalEngine — passed via lock-free atomic
- Use `juce::AbstractFifo` ring buffer to pass envelope level from audio thread to UI thread (60fps timer)
- Glow effect: draw star lines twice — once thick+transparent for bloom, once thin+opaque for core

---

### Engine Panel Icons

Replace generic dot indicators with celestial glyphs, drawn in `juce::Graphics`:

| Engine | Icon | Color when active |
|---|---|---|
| HAUNT VERB | Crescent moon ☽ | Ice blue `#a8c4e8` |
| SHIMMER | Six-point star ✦ | Aged gold `#c9a84c` |
| PITCH GHOST | Four-point star ✧ | Ghost white `#e8e8f0` |

Each icon sits below the engine knobs. When the engine is inactive: dim, desaturated. When active: full color with a soft radial glow behind it (drawn as a blurred circle underneath the glyph).

---

### LiminalLookAndFeel

```cpp
class LiminalLookAndFeel : public juce::LookAndFeel_V4 {
public:
    LiminalLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool highlighted, bool down) override;

    // ── Color Palette ──────────────────────────────────────────
    // Inspired by celestial tarot: deep cobalt field, gold geometry,
    // ice blue and ghost white accents. Functional and clean.

    static constexpr auto COBALT         = juce::Colour(0xff0a0f2e); // background
    static constexpr auto COBALT_MID     = juce::Colour(0xff1a2050); // center gradient
    static constexpr auto GOLD           = juce::Colour(0xffc9a84c); // primary accent, star lines
    static constexpr auto GOLD_DIM       = juce::Colour(0xff6b5a28); // inactive gold
    static constexpr auto ICE_BLUE       = juce::Colour(0xffa8c4e8); // HAUNT VERB active
    static constexpr auto GHOST_WHITE    = juce::Colour(0xffe8e8f0); // PITCH GHOST active
    static constexpr auto PANEL_BORDER   = juce::Colour(0xff2a3060); // subtle engine panel edges
    static constexpr auto KNOB_TRACK     = juce::Colour(0xff1e2448); // rotary slider track
    static constexpr auto TEXT_PRIMARY   = juce::Colour(0xffe8e8f0); // labels
    static constexpr auto TEXT_DIM       = juce::Colour(0xff6070a0); // secondary labels

    // ── Knob Style ─────────────────────────────────────────────
    // Rotary sliders: dark track ring, gold filled arc, small gold
    // center dot. No pointer line — arc length communicates value.
    // Font: clean geometric sans (Inter or similar)
};
```

---

### Typography

- **Plugin title "LIMINAL":** Wide-tracked uppercase, ghost white, top left. Suggest: Inter ExtraLight or similar geometric sans at ~18pt with letter-spacing 0.3em
- **Engine labels (HAUNT VERB / SHIMMER / PITCH GHOST):** Small caps or uppercase, gold, 10pt
- **Knob labels:** Ghost white, 9pt, centered below knob
- **Parameter values (on hover):** Ice blue tooltip, 9pt

---

### Knob Design

```
Rotary slider appearance:
- Outer track ring: KNOB_TRACK color, full 270° arc
- Filled arc: GOLD, from start to current position
- Center: small filled circle in COBALT with a GOLD dot at center
- Size: 48px diameter for main knobs, 36px for engine sub-knobs
- No pointer line — the arc IS the value indicator
- On hover: arc brightens to full GOLD, value tooltip appears in ICE_BLUE
```

---

### Ramp Bar (bottom strip)

```
[RAMP A label] ─────────●──────── [RAMP B label]   [TIME knob]  [LATCH toggle]

- Horizontal slider for morph position between state A and state B
- Track: PANEL_BORDER color
- Thumb: gold circle with subtle glow
- LATCH toggle: small rectangular button, lights gold when engaged
- RAMP A / B labels: dim gold, clicking each stores current parameter state
```

---

## Pamplejuce CMake Configuration

In `CMakeLists.txt`, update the following sections after cloning Pamplejuce:

```cmake
set(PLUGIN_NAME "LIMINAL")
set(PLUGIN_VERSION "0.1.0")
set(PLUGIN_MANUFACTURER "Ament Audio")
set(PLUGIN_MANUFACTURER_CODE "AmAu")
set(PLUGIN_CODE "Lmnl")
set(PLUGIN_FORMATS VST3 AU Standalone)

# DSP sources
target_sources(${PLUGIN_NAME} PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/dsp/EnvelopeFollower.cpp
    source/dsp/LiminalEngine.cpp
    source/dsp/HauntVerb.cpp
    source/dsp/Shimmer.cpp
    source/dsp/PitchGhost.cpp
    source/modulation/RampSystem.cpp
    source/modulation/ModMatrix.cpp
    source/ui/LiminalLookAndFeel.cpp
    source/ui/ThresholdDisplay.cpp
    source/ui/EnginePanel.cpp
    source/ui/KnobComponent.cpp
)

# JUCE modules needed
target_link_libraries(${PLUGIN_NAME} PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_graphics
)
```

---

## JUCE DSP Modules in Use

| Module | Used For |
|---|---|
| `juce::dsp::DelayLine` | HauntVerb diffusion lines |
| `juce::dsp::StateVariableTPTFilter` | HauntVerb high-pass sweep |
| `juce::dsp::Oscillator` | LFO in ModMatrix |
| `juce::dsp::ProcessSpec` | prepare() throughout |
| `juce::AudioProcessorValueTreeState` | All parameters + undo/DAW automation |
| `juce::dsp::Gain` | Wet/dry mix stages |

---

## Coding Conventions

- All DSP classes implement `prepare(const juce::dsp::ProcessSpec&)` before use
- `processBlock` must be real-time safe: no allocations, no locks, no exceptions
- All heap allocations happen in `prepareToPlay` / `prepare()`
- Use `juce::ScopedNoDenormals` at the top of `processBlock`
- Parameter smoothing: use `juce::SmoothedValue<float>` for all APVTS reads in the audio thread
- Thread safety: APVTS handles message/audio thread boundary — do not share raw pointers across threads

---

## Implementation Priority Order

1. `EnvelopeFollower` — foundation everything depends on
2. `LiminalEngine` skeleton — threshold detection + blend logic
3. `HauntVerb` — most conventional DSP, good starting point
4. `PluginProcessor` + `PluginEditor` scaffolding — get audio flowing
5. `Shimmer` — pitch shifting is complex, build on stable core
6. `PitchGhost` — most novel, save for stable base
7. `RampSystem` + `ModMatrix` — modulation layer last
8. Full UI polish — `ThresholdDisplay`, `LiminalLookAndFeel`

---

## Known Complexity Areas

- **PitchGhost capture trigger:** Needs careful zero-crossing detection to avoid clicks when capturing the snapshot buffer. Use a short fade-in on capture playback.
- **Shimmer pitch shifter:** Recommend a phase vocoder approach (matching FREECODER's DNA). Can stub with `juce::dsp::ProcessorChain` pitch shift as placeholder.
- **ThresholdDisplay threading:** Waveform display reads envelope data from audio thread. Use a lock-free ring buffer (`juce::AbstractFifo`) to pass data to the UI thread safely.
- **Slew rate at sample rate changes:** Recompute slew coefficients in `prepareToPlay`.

---

## Testing

```cpp
// tests/LiminalTests.cpp

// 1. EnvelopeFollower: feed silence → expect 0.0 output
// 2. EnvelopeFollower: feed 1.0 sine → expect envelope converges to ~1.0
// 3. LiminalEngine: above threshold → expect blend = 0.0
// 4. LiminalEngine: below threshold → expect blend > 0.0
// 5. HauntVerb: silence in → silence out (no self-oscillation at default settings)
// 6. Shimmer: crystallize=0 → output decays; crystallize=1 → output holds
// 7. RampSystem: trigger → position advances from 0.0 to 1.0 over rampTime
// 8. No denormals in processBlock under silence
```

---

*LIMINAL — Ament Audio — Claude.md v1.1 — UI updated to celestial geometry hybrid direction*
