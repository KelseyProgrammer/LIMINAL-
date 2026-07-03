# LIMINAL — Claude.md
## Ament Audio | VST3/AU/CLAP Plugin | JUCE 8 + Pamplejuce

---

## Project Overview

**LIMINAL** is a threshold-based negative-space effect plugin. It monitors the input signal's amplitude envelope and activates HAUNT VERB, SHIMMER, and PITCH GHOST in the space *between* notes: decays, silences, and breaths.

**Tagline:** *"The effect that wakes up when you stop playing."*
**Version:** 0.2.0
**Format:** VST3 / AU / CLAP (macOS + Windows + Linux)
**Template:** [Pamplejuce](https://github.com/sudara/pamplejuce) (JUCE 8.x, CMake 3.25+)
**Plugin Type:** `AudioProcessor` — stereo effect (mono-compatible)
**Model:** claude-fable-5 (configured in `.claude/settings.json`)

---

## Brand & Aesthetic

LIMINAL is the third plugin in the Ament Audio trilogy:

| Plugin | Domain | Character |
|---|---|---|
| FREECODER | Spectral content | Morphs timbres |
| HALATION | The bloom/halo | Diffuses and glows |
| LIMINAL | Negative space | Haunts the silence |

**Color palette:** deep cobalt `#0a0f2e`, aged gold `#c9a84c`, ice blue `#a8c4e8`, ghost white `#e8e8f0`

---

## Repository Structure

```
LIMINAL/
├── CMakeLists.txt
├── CMakePresets.json
├── Claude.md                         ← this file
├── AGENTS.md                         ← mirror of Claude.md for agent context
├── SETUP_GUIDE.md
├── VERSION                           ← 0.2.0
├── .claude/
│   └── settings.json                 ← model: claude-fable-5
├── .github/
│   └── workflows/
│       ├── build_and_test.yml
│       └── nightly.yml
├── source/
│   ├── PluginProcessor.h / .cpp
│   ├── PluginEditor.h / .cpp
│   ├── dsp/
│   │   ├── EnvelopeFollower.h / .cpp
│   │   ├── LiminalEngine.h / .cpp
│   │   ├── HauntVerb.h / .cpp
│   │   ├── Shimmer.h / .cpp
│   │   └── PitchGhost.h / .cpp
│   ├── modulation/
│   │   ├── RampSystem.h / .cpp
│   │   └── ModMatrix.h / .cpp
│   └── ui/
│       ├── LiminalLookAndFeel.h / .cpp
│       ├── ThresholdDisplay.h / .cpp
│       ├── EnginePanel.h / .cpp
│       └── KnobComponent.h / .cpp
├── tests/
│   └── LiminalTests.cpp              ← 30 tests, all passing
└── assets/
    ├── fonts/
    └── images/
        └── liminal_bg.png            ← 950×668 reference artwork (UI background)
```

---

## Implementation Status — ALL COMPLETE

All DSP engines, modulation, UI, and preset systems are implemented and tested. The plugin builds and runs on macOS. The remaining gap to a signed distribution release is Apple Developer / Azure code signing credentials (not a code problem).

---

## Class Architecture

```
PluginProcessor (AudioProcessor)
    ├── AudioProcessorValueTreeState (APVTS)
    ├── EnvelopeFollower
    ├── LiminalEngine
    │     ├── HauntVerb
    │     ├── Shimmer
    │     └── PitchGhost
    ├── RampSystem
    └── ModMatrix

PluginEditor (AudioProcessorEditor)
    ├── ContentComponent (950×668, scaled via AffineTransform)
    │     ├── LiminalLookAndFeel
    │     ├── ThresholdDisplay (star animation, 297,20,360,220)
    │     ├── EnginePanel × 3
    │     └── KnobComponent × 9+
    └── [scale persisted in apvts.state "uiScale"]
```

---

## DSP Architecture (Shipped)

### EnvelopeFollower

One-pole IIR envelope follower. Uses normalized dB units: 0 = −60dB, 1 = 0dBFS. Per-sample mono max-abs detection.

```cpp
void prepare(double sampleRate);
float process(float inputSample);    // call per sample
float getCurrentLevel() const;       // 0.0–1.0 normalized
void setAttack(float attackMs);
void setRelease(float releaseMs);
```

- Attack: ~5ms default
- Release: ~200ms default (user-adjustable via SLEW)

---

### LiminalEngine

Master coordinator. Engines render **parallel wet-only buses** via `renderWet(in, wet, blend)`. Shimmer pipes 0.85 of its output into HauntVerb's input. Final output: `dry + wetSum * mix`.

```cpp
void prepare(const juce::dsp::ProcessSpec& spec);
void process(juce::AudioBuffer<float>& buffer, float envelopeLevel);
void setThreshold(float t);    // 0.0–1.0 normalized dB
void setSlew(float slewMs);
void setDepth(float depth);
void setMix(float mix);
void setInvertMode(bool invert);
void setTone(float tone);      // −1 dark LP, +1 bright HP
float getCurrentBlend() const;
```

**Blend logic:**
```
if (!invertMode && envelopeLevel < threshold):
    targetBlend = (1 - envelopeLevel/threshold) * depth
elif (invertMode && envelopeLevel >= threshold):
    targetBlend = ((envelopeLevel - threshold) / (1 - threshold)) * depth
else:
    targetBlend = 0

currentBlend += (targetBlend - currentBlend) * slewRate  // one-pole slew
```

Tone: one-pole LP (dark) or HP (bright) applied post-mix. Bypassed at tone=0.

---

### HauntVerb

Diffusion + cross-coupled tank reverb. Character opens as signal fades.

```cpp
void prepare(const juce::dsp::ProcessSpec& spec);
void renderWet(const juce::AudioBuffer<float>& in,
               juce::AudioBuffer<float>& wet, float blendFactor);
void setHaunt(float amount);        // 0–1, maps to tank feedback 0.55–0.95
void setEnvelopeLevel(float level);
```

**Architecture:**
- Input blend-gated: `in * (1 - envelopeLevel)` so tail rings out after signal stops
- 4-stage allpass diffusion per channel (coefficients: 0.52, 0.42, 0.33, 0.25; one-pole LP at 0.28 in allpass feedback to prevent metallic ringing)
- Cross-coupled stereo tank: 97ms (L) / 113ms (R) delay lines, sine-modulated ±0.9ms @ 0.23Hz
- Damping LP in tank feedback
- Pre-delay stretches 10ms → 60ms as envelope drops
- HP sweeps ~80Hz → ~4kHz in deep silence (counterintuitive brightening = "exhale" quality)
- HAUNT knob maps to tank feedback 0.55–0.95

---

### Shimmer

OLA pitch-shifter cascade → damped feedback delay. Crystallize = real freeze.

```cpp
void prepare(const juce::dsp::ProcessSpec& spec);
void renderWet(const juce::AudioBuffer<float>& in,
               juce::AudioBuffer<float>& wet, float blendFactor);
void setCrystallize(float amount);   // 0–1
void setInterval(int semitones);
void setFeedback(float feedback);    // 0–0.6
bool isFrozen() const;
```

**Architecture:**
- Two OLA pitch-shifter voices: voice 1 = user interval, voice 2 = interval + 7 semitones (fifth avoids harsh 2-octave stacking)
- 4096-sample grains, 50% offset, distance-based Hann window (always sums to 1.0)
- writePos starts one full grain ahead of readPos to prevent grinding on pitch ratios > 1
- One-pole LP on output to smooth grain boundaries
- ±3-cent L/R detune per voice for stereo width
- Cascade via 340ms damped feedback delay
- **Crystallize:** freeze ring (2s) latches on amplitude rise past 0.08; crossfaded loop playback; sustains at 1.0 independent of blend; decay 0.5–8.5s when crystallize < 1

**Interval map:**
```
0: Oct (+12)       2: Oct+5th (+19)    4: Tritone (+6)
1: 5th (+7)        3: m2 (+1)
```

---

### PitchGhost

Three round-robin ghost voices. Captures signal snapshot at threshold crossing; replays as detuning phantom.

```cpp
void prepare(const juce::dsp::ProcessSpec& spec);
void renderWet(juce::AudioBuffer<float>& wet, float blendFactor);
void pushAudio(const juce::AudioBuffer<float>& input);   // called each block
void triggerCapture();                                    // called at threshold crossing
void setPossession(float amount);    // 0=alien drift, 1=tight double
void setDriftRate(float rateHz);
void setDriftDirection(int dir);     // -1=down, 0=wander, 1=up
void setGhostDecay(float decayMs);
```

**Architecture:**
- Ring buffer holds last ~256 samples; `triggerCapture()` snapshots into per-voice capture buffer
- Crossfade window near buffer boundaries (fadeZone = min(256, captureLength×0.05)) prevents clicks at loop wrap
- Drift: up to ±1200 cents at Possession 0; near-zero at Possession 1
- 3 voices play as a choir — each capture adds to the ensemble
- Fade-in on first render block after capture

---

### RampSystem

Morphs between two saved parameter states (Chase Bliss-style).

```cpp
void setRampTime(float timeMs, double sampleRate);
void trigger();                // reverses direction A↔B
float getPosition() const;    // 0=state A, 1=state B
void process(int numSamples);
```

---

### ModMatrix

Routes LFO and envelope to parameter destinations.

```cpp
enum Source  { LFO, ENVELOPE };
enum Dest    { THRESHOLD, DEPTH, HAUNT, CRYSTALLIZE, POSSESSION, TONE };
void setRouting(Source src, Dest dest, float amount);   // amount: −1 to +1
float getModValue(Dest dest) const;
void process(float lfoValue, float envelopeValue);
```

---

## Parameter Model (APVTS)

All parameters registered in `PluginProcessor.cpp` → `createParameterLayout()`:

| ID | Name | Range | Default |
|---|---|---|---|
| threshold | Threshold | 0–1 (0.01 step) | 0.3 |
| slew | Slew | 5–2000ms (log) | 200 |
| depth | Depth | 0–1 | 1.0 |
| haunt | Haunt | 0–1 | 0.5 |
| crystallize | Crystallize | 0–1 | 0.0 |
| possession | Possession | 0–1 | 0.5 |
| interval | Interval | Oct/5th/Oct+5th/m2/Tritone | Oct |
| tone | Tone | −1 to +1 | 0.0 |
| mix | Mix | 0–1 | 1.0 |
| invertMode | Invert Mode | bool | false |
| latch | Latch | bool | false |
| sidechain | Sidechain | bool | false |
| autoRamp | Auto Ramp | bool | false |
| lfoSync | LFO Sync | bool | false |
| rampSync | Ramp Sync | bool | false |
| rampA | Ramp A | 0–1 | 0.0 |
| rampB | Ramp B | 0–1 | 1.0 |
| rampTime | Ramp Time | 100–10000ms (log) | 2000 |
| driftRate | Drift Rate | 0.01–2.0 Hz | 0.1 |
| driftDir | Drift Dir | −1/0/+1 | 0 |
| ghostDecay | Ghost Decay | 100–5000ms | 1000 |

---

## Audio Processing Flow

```cpp
void PluginProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    syncParametersFromAPVTS();   // SmoothedValue::skip(numSamples) per param

    // Envelope follower (mono max-abs, per sample; sidechain bus if enabled)
    float envelopeLevel = 0.f;
    for (int s = 0; s < buffer.getNumSamples(); ++s)
        envelopeLevel = envelopeFollower.process(src[s]);

    // MIDI: note-on → ghost capture
    for (const auto meta : midi)
        if (meta.getMessage().isNoteOn())
            liminalEngine.triggerGhostCapture(buffer);

    // Modulation
    modMatrix.process(lfoValue, envelopeLevel);
    rampSystem.process(buffer.getNumSamples());

    // Main engine (parallel wet buses internally)
    liminalEngine.process(buffer, envelopeLevel);
}
```

**Key real-time safety rules:**
- No allocations in processBlock — all heap work in `prepareToPlay` / `prepare()`
- `juce::SmoothedValue<float>` for all APVTS reads; call `skip(numSamples)` not `getNextValue()` once per block (was a critical bug; now fixed)
- `juce::ScopedNoDenormals` at top of processBlock
- Lock-free `juce::AbstractFifo` ring buffer for UI envelope data (60fps timer)

---

## UI Architecture (Shipped)

**Window:** 950×668px ContentComponent, editor resizable 50–200% via AffineTransform (aspect-locked, scale stored in `apvts.state["uiScale"]`).

**Background:** `assets/images/liminal_bg.png` (950×668) drawn 1:1. All controls pixel-aligned over the painted positions.

**Control centres (ContentComponent coordinates):**

| Row | y | x positions | Knob dia |
|---|---|---|---|
| Top (THRESHOLD/SLEW/DEPTH/TONE/MIX) | 302 | 103/278/453/629/803 | 56px |
| Engine (HAUNT/CRYSTALLIZE/POSSESSION) | 401 | 167/472/779 | 52px |
| Mod row | 526 | 105/289/473/656/841 | 44px |

Engine panel bounds (x,y,w,h): `(10,355,300,100)` / `(320,355,300,100)` / `(635,355,300,100)`

ThresholdDisplay: bounds `(297,20,360,220)`, star centre `(477,130)`, draggable threshold ring r=58–104px

Additional controls: INV `(897,294,32,32)`, SC `(897,330,32,18)`, A `(45,604)`, B `(771,604)`, LATCH `(803,602)`, AUTO `(803,576)`, rampSync `(698,575)`, lfoSync `(75,562)`, TIME centre `(898,604)`

Ghost panel sub-controls: Drift knob `(40,30,30,30)`, dir combo `(12,h−28,80,20)`

---

## CelestialThresholdDisplay

Six-pointed star (hexagram) in `juce::Graphics`. Scales with engine blend. Axis pairs light in engine colors:
- HAUNT VERB → ice blue `#a8c4e8`
- SHIMMER → aged gold `#c9a84c`
- PITCH GHOST → ghost white `#e8e8f0`

Threshold ring: draggable, radius maps to THRESHOLD param. Envelope ring: animated to live level.

Glow: star lines drawn twice (thick+transparent bloom, then thin+opaque core).

---

## LiminalLookAndFeel — Color Palette

```cpp
static constexpr auto COBALT      = juce::Colour(0xff0a0f2e);
static constexpr auto COBALT_MID  = juce::Colour(0xff1a2050);
static constexpr auto GOLD        = juce::Colour(0xffc9a84c);
static constexpr auto GOLD_DIM    = juce::Colour(0xff6b5a28);
static constexpr auto ICE_BLUE    = juce::Colour(0xffa8c4e8);
static constexpr auto GHOST_WHITE = juce::Colour(0xffe8e8f0);
static constexpr auto PANEL_BORDER= juce::Colour(0xff2a3060);
static constexpr auto KNOB_TRACK  = juce::Colour(0xff1e2448);
static constexpr auto TEXT_PRIMARY= juce::Colour(0xffe8e8f0);
static constexpr auto TEXT_DIM    = juce::Colour(0xff6070a0);
```

Rotary slider: dark track ring (full 270°), gold filled arc, COBALT center + GOLD dot. No pointer line.

---

## Presets

24 factory presets in `PluginProcessor::getProgramName()` / `setCurrentProgram()`. Accessible via DAW program list.

---

## Tests

`tests/LiminalTests.cpp` — 30 tests, all passing.

Build + test:
```bash
cmake --build Builds --config Release
cd Builds && ctest --verbose --output-on-failure
```

Coverage: EnvelopeFollower, LiminalEngine blend/depth/invert/tone, HauntVerb tank tail, Shimmer crystallize freeze, PitchGhost choir, RampSystem, integration (atmosphere appears after sound stops).

---

## CMake Config

```cmake
set(PROJECT_NAME "LIMINAL")
set(PRODUCT_NAME "LIMINAL")
set(COMPANY_NAME "Ament Audio")
set(BUNDLE_ID "com.amentagudio.liminal")
set(FORMATS Standalone AU VST3)   # CLAP via clap-juce-extensions
```

JUCE modules: `juce_audio_processors`, `juce_audio_utils`, `juce_dsp`, `juce_gui_basics`, `juce_graphics`
JUCE DSP in use: `DelayLine`, `StateVariableTPTFilter`, `Oscillator` (LFO), `ProcessSpec`, `SmoothedValue`, `AbstractFifo`

Note: `NEEDS_MIDI_INPUT` must be set for MIDI note-on → ghost capture in AU/VST3.

---

## Coding Conventions

- All DSP classes: `prepare(const juce::dsp::ProcessSpec&)` before use
- `processBlock` real-time safe: no allocations, no locks, no exceptions
- `SmoothedValue::skip(numSamples)` — not `getNextValue()` once per block
- APVTS handles message/audio thread boundary — no raw pointer sharing
- UI ↔ audio: lock-free `AbstractFifo` ring buffer for envelope data

---

## Release Status

**Done:** All DSP engines, full APVTS model, ModMatrix, RampSystem, sidechain, invert mode, 24 presets, artwork UI, star animation, 30 passing tests.

**Blockers for signed public release:**
1. macOS code signing — needs `DEV_ID_APP_CERT` GitHub secret (Apple Developer)
2. macOS notarization — needs Apple notarization credentials
3. Windows code signing — needs Azure Trusted Signing secrets
4. Pluginval strictness-10 verified CI pass

**Nice-to-have:** output auto-gain, HauntVerb SIZE control, in-plugin preset browser, CPU profiling pass.

---

*LIMINAL — Ament Audio — Claude.md v2.0 — 2026-07-02*
