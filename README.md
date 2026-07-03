# LIMINAL
### Ament Audio — VST3 / AU / CLAP

*The effect that wakes up when you stop playing.*

LIMINAL is a threshold-based negative-space processor. It listens to your input signal's amplitude envelope and activates a suite of atmospheric engines — **HAUNT VERB**, **SHIMMER**, and **PITCH GHOST** — in the space *between* notes: decays, silences, and breaths. The louder the silence, the more alive LIMINAL becomes.

---

## What It Does

Most effects react to what you play. LIMINAL reacts to what you *don't* play.

Set a threshold. The moment your signal drops below it, three engines wake up:

- **HAUNT VERB** — A diffusion reverb whose character opens as the signal fades. The high-pass sweeps upward in silence, creating a bright, exhale quality. The cross-coupled delay tank sustains a tail up to 10 seconds.
- **SHIMMER** — Overlap-add pitch shifters (two voices in cascade) feeding back through a damped delay line. **Crystallize** latches the output into a sustained drone independent of further input — a freeze that holds whatever was playing at the crossing moment.
- **PITCH GHOST** — Three round-robin ghost voices that capture a micro-snapshot of the signal at the threshold crossing and replay it as a detuned phantom. **Possession** controls how far the ghost drifts from the source pitch — from an alien ±1200-cent wander to a tight shimmer double.

LIMINAL ships with **24 factory presets** accessible via the DAW's program list.

---

## Engines

### HAUNT VERB
| Parameter | Range | Description |
|---|---|---|
| HAUNT | 0–1 | Tank feedback (0.55–0.95). Higher = longer, denser tail. |

- 4-stage allpass diffusion network → cross-coupled stereo tank (97ms / 113ms)
- Sine-modulated tap (±0.9ms @ 0.23 Hz) prevents metallic resonance
- Damping LP inside feedback loop
- Pre-delay stretches 10ms → 60ms as envelope drops
- HP sweeps 80Hz → 4kHz in deep silence

### SHIMMER
| Parameter | Range | Description |
|---|---|---|
| CRYSTALLIZE | 0–1 | 0 = decays normally; 1 = freeze/sustained drone |
| INTERVAL | Oct / 5th / Oct+5th / m2 / Tritone | Pitch shift interval for voice 1 |

- Overlap-add (OLA) pitch shifter, 4096-sample grains, 50% overlap
- Voice 1: selected interval. Voice 2: interval + fifth (richer stacking)
- ±3-cent L/R detune for stereo width
- Crystallize: freeze ring-buffer latches on signal rise >0.08, crossfade loop, sustains at 1.0 independent of blend

### PITCH GHOST
| Parameter | Range | Description |
|---|---|---|
| POSSESSION | 0–1 | 0 = alien free-drift; 1 = tight double |
| DRIFT DIR | ↓ / wander / ↑ | Direction of pitch drift |

- 3 round-robin ghost voices triggered at threshold crossing
- Captures 256-sample snapshot with fade-in to prevent clicks
- Drift up to ±1200 cents at Possession 0; near-zero at Possession 1
- Voices play as a choir — each capture adds to the ensemble

---

## Global Controls

| Parameter | Range | Description |
|---|---|---|
| THRESHOLD | 0–1 | Normalized dBFS (0 = −60dB, 1 = 0dBFS). Draggable ring in the display. |
| SLEW | 5–2000ms | Smoothing time on the blend transition |
| DEPTH | 0–1 | Scales the wet blend factor — how intensely engines activate |
| TONE | −1 to +1 | −1 = dark LP; +1 = bright HP |
| MIX | 0–1 | Final wet/dry balance |
| INVERT | toggle | Engines activate *above* threshold instead of below |
| SIDECHAIN | toggle | Use sidechain bus as the envelope source |

### Modulation
| Parameter | Description |
|---|---|
| RAMP A / B | Store two parameter snapshots; morph between them |
| RAMP TIME | Morph duration (100ms – 10s) |
| LATCH | Hold current engine output regardless of envelope changes |
| AUTO RAMP | Trigger A→B morph automatically on threshold crossing |
| LFO SYNC | Lock LFO rate to host BPM |
| RAMP SYNC | Lock ramp time to host BPM |

---

## UI

The plugin window is fixed at **950 × 668px**, resizable 50–200% (aspect-locked). The background artwork is the reference image rendered 1:1; controls are pixel-aligned over the painted positions.

The central **Celestial Threshold Display** shows a six-pointed star that expands and brightens as the engines activate. Each engine pair illuminates in its signature color:
- HAUNT VERB → ice blue `#a8c4e8`
- SHIMMER → aged gold `#c9a84c`
- PITCH GHOST → ghost white `#e8e8f0`

The **threshold ring** is draggable. A second animated ring shows the live envelope level — when it crosses inside the threshold ring, the engines wake.

---

## Building

### Requirements

- **macOS:** Xcode 14+, CMake 3.25+, Ninja
- **Windows:** Visual Studio 2022, CMake 3.25+, Ninja
- **Linux:** Clang, CMake 3.25+, Ninja, JUCE Linux deps (see CI workflow)

### Steps

```bash
git clone https://github.com/KelseyProgrammer/LIMINAL-
cd LIMINAL-
git submodule update --init --recursive

# macOS / Linux
cmake -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build Builds --config Release

# Windows
cmake -B Builds -DCMAKE_BUILD_TYPE=Release
cmake --build Builds --config Release
```

Built artifacts appear in `Builds/LIMINAL_artefacts/Release/`.

### Running Tests

```bash
cd Builds && ctest --verbose --output-on-failure
```

30 tests covering EnvelopeFollower, LiminalEngine blend/depth/invert/tone, HauntVerb tank tail, Shimmer crystallize freeze, PitchGhost ghost choir, RampSystem, and the full integration chain.

---

## Install

### macOS (AU / VST3)

Copy from `Builds/LIMINAL_artefacts/Release/`:

| Format | Destination |
|---|---|
| AU | `~/Library/Audio/Plug-Ins/Components/` |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` |

Rescan plugins in your DAW after copying.

> **Note:** macOS Gatekeeper — if you built from source, the binary is unsigned. Right-click → Open the first time to bypass the quarantine dialog, or run `xattr -cr path/to/LIMINAL.component`.

### Windows (VST3)

Copy `LIMINAL.vst3` to `C:\Program Files\Common Files\VST3\`.

---

## Presets

LIMINAL ships 24 factory presets accessible via the DAW's program list (not a built-in browser — use your DAW's preset mechanism to step through them).

Representative presets:
- **Deep Space Exhale** — full HAUNT + crystallize drone on long decays
- **Ghost Choir** — three-voice possession choir with slow drift
- **Tension Hold** — tritone shimmer + tight ghost, near-zero slew
- **Breath** — subtle bloom, fast slew, low depth — barely perceptible

---

## Version History

| Version | Notes |
|---|---|
| 0.2.0 | Documentation pass, version alignment, Fable model config |
| 0.1.0 | DSP overhaul: real reverb tank, Crystallize freeze, Ghost choir, sidechain, 24 presets, artwork UI |
| 0.0.2 | Celestial UI, knob-star effects |
| 0.0.1 | Initial scaffold |

---

## Known Limitations

- No built-in preset browser (presets are DAW program list only)
- Standalone CPU ~80% on older x86 hardware with all engines at full blend — GPU-accelerated UI repaints and OLA shifters are the main cost
- macOS binaries are unsigned in CI until Apple Developer credentials are configured in GitHub secrets

---

## Part of the Ament Audio Trilogy

| Plugin | Domain | Character |
|---|---|---|
| FREECODER | Spectral content | Morphs timbres |
| HALATION | The bloom / halo | Diffuses and glows |
| **LIMINAL** | **Negative space** | **Haunts the silence** |

---

*Built on [Pamplejuce](https://github.com/sudara/pamplejuce) — JUCE 8.x, CMake, Catch2, GitHub Actions CI*
