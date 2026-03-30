# LIMINAL — Setup Guide
## Ament Audio | JUCE + Pamplejuce | VST3 / AU

---

## Prerequisites

Before starting, ensure the following are installed:

### All Platforms
- [CMake](https://cmake.org/download/) **3.22 or higher**
- [Git](https://git-scm.com/)
- [JUCE 7.x](https://github.com/juce-framework/JUCE) (fetched automatically via Pamplejuce)

### macOS
- **Xcode 14+** with Command Line Tools
  ```bash
  xcode-select --install
  ```
- macOS 12 Monterey or higher (for AU + VST3 targets)

### Windows
- **Visual Studio 2022** (Community edition is fine)
  - Install with: **Desktop development with C++** workload
- Windows 10 or 11

---

## Step 1 — Clone Pamplejuce

Pamplejuce is the project template. Clone it as the base for LIMINAL.

```bash
git clone https://github.com/sudara/pamplejuce.git LIMINAL
cd LIMINAL
```

---

## Step 2 — Initialize Submodules

Pamplejuce uses JUCE and Catch2 as submodules.

```bash
git submodule update --init --recursive
```

This will pull JUCE into `lib/JUCE` and Catch2 into `lib/Catch2`.

---

## Step 3 — Create Your GitHub Repo

1. Go to [github.com/new](https://github.com/new)
2. Name it `LIMINAL`
3. Set it to **Private** (recommended until release)
4. Do **not** initialize with README or .gitignore (Pamplejuce has these)

Then push the local Pamplejuce clone up:

```bash
git remote set-url origin https://github.com/YOUR_USERNAME/LIMINAL.git
git push -u origin main
```

---

## Step 4 — Configure CMakeLists.txt

Open `CMakeLists.txt` in the project root and update the plugin identity block near the top:

```cmake
set(PLUGIN_NAME             "LIMINAL")
set(PLUGIN_VERSION          "0.1.0")
set(PLUGIN_MANUFACTURER     "Ament Audio")
set(PLUGIN_MANUFACTURER_CODE "AmAu")
set(PLUGIN_CODE             "Lmnl")
set(PLUGIN_DESCRIPTION      "Negative space effect — the effect that wakes up when you stop playing")
set(PLUGIN_FORMATS          VST3 AU Standalone)
```

> **Note:** `PLUGIN_MANUFACTURER_CODE` must be exactly 4 characters. `PLUGIN_CODE` must be exactly 4 characters and unique to this plugin.

---

## Step 5 — Create the Source File Structure

From the project root, run:

```bash
mkdir -p source/dsp
mkdir -p source/modulation
mkdir -p source/ui
mkdir -p assets/fonts
```

Then create placeholder `.h` and `.cpp` files for each class (Claude Code will populate these):

**DSP layer:**
```bash
touch source/dsp/EnvelopeFollower.h   source/dsp/EnvelopeFollower.cpp
touch source/dsp/LiminalEngine.h      source/dsp/LiminalEngine.cpp
touch source/dsp/HauntVerb.h          source/dsp/HauntVerb.cpp
touch source/dsp/Shimmer.h            source/dsp/Shimmer.cpp
touch source/dsp/PitchGhost.h         source/dsp/PitchGhost.cpp
```

**Modulation layer:**
```bash
touch source/modulation/RampSystem.h  source/modulation/RampSystem.cpp
touch source/modulation/ModMatrix.h   source/modulation/ModMatrix.cpp
```

**UI layer:**
```bash
touch source/ui/LiminalLookAndFeel.h  source/ui/LiminalLookAndFeel.cpp
touch source/ui/ThresholdDisplay.h    source/ui/ThresholdDisplay.cpp
touch source/ui/EnginePanel.h         source/ui/EnginePanel.cpp
touch source/ui/KnobComponent.h       source/ui/KnobComponent.cpp
```

---

## Step 6 — Add Sources to CMakeLists.txt

In `CMakeLists.txt`, find the `target_sources` block (Pamplejuce has a placeholder) and replace it:

```cmake
target_sources(${PLUGIN_NAME}
    PRIVATE
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
```

Also ensure these JUCE modules are linked:

```cmake
target_link_libraries(${PLUGIN_NAME}
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_core
)
```

---

## Step 7 — Configure CMake & Build

### macOS

```bash
cmake -B build -G Xcode
cmake --build build --config Debug
```

Or open in Xcode:
```bash
open build/LIMINAL.xcodeproj
```

### Windows

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

Or open `build/LIMINAL.sln` in Visual Studio 2022.

### Verify Build

After a successful build, the plugin binaries will be at:

**macOS:**
```
build/LIMINAL_artefacts/Debug/VST3/LIMINAL.vst3
build/LIMINAL_artefacts/Debug/AU/LIMINAL.component
```

**Windows:**
```
build\LIMINAL_artefacts\Debug\VST3\LIMINAL.vst3
```

---

## Step 8 — Install for DAW Testing

### macOS — VST3
```bash
cp -R build/LIMINAL_artefacts/Debug/VST3/LIMINAL.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

### macOS — AU
```bash
cp -R build/LIMINAL_artefacts/Debug/AU/LIMINAL.component ~/Library/Audio/Plug-Ins/Components/
```
Then re-scan in your DAW (Ableton, Logic, etc.)

### Windows — VST3
```
Copy LIMINAL.vst3 → C:\Program Files\Common Files\VST3\
```

---

## Step 9 — Add Claude.md to Repo Root

Copy `Claude.md` into the repo root so Claude Code has it available immediately when the project is opened:

```bash
cp Claude.md /path/to/LIMINAL/Claude.md
git add Claude.md SETUP_GUIDE.md
git commit -m "Add Claude.md and SETUP_GUIDE.md"
git push
```

---

## Step 10 — Open in Claude Code

1. Open your terminal
2. Navigate to the LIMINAL repo root:
   ```bash
   cd /path/to/LIMINAL
   ```
3. Launch Claude Code:
   ```bash
   claude
   ```
4. Claude Code will read `Claude.md` automatically. Start with:

   > *"Read Claude.md and begin implementing LIMINAL, starting with EnvelopeFollower.h and EnvelopeFollower.cpp, then LiminalEngine."*

---

## Recommended Implementation Order for Claude Code

Feed Claude Code tasks in this sequence for the smoothest build progression:

| Step | Task |
|---|---|
| 1 | Implement `EnvelopeFollower` (h + cpp) |
| 2 | Implement `LiminalEngine` skeleton — threshold detection + blend only |
| 3 | Scaffold `PluginProcessor` with APVTS parameter layout |
| 4 | Scaffold `PluginEditor` — empty window, confirm audio passes through |
| 5 | Implement `HauntVerb` |
| 6 | Implement `Shimmer` |
| 7 | Implement `PitchGhost` |
| 8 | Implement `RampSystem` |
| 9 | Implement `ModMatrix` |
| 10 | Build `LiminalLookAndFeel` and `KnobComponent` |
| 11 | Build `ThresholdDisplay` (lock-free ring buffer + waveform) |
| 12 | Build `EnginePanel` (three sub-panels with activity indicators) |
| 13 | Wire full UI to APVTS |
| 14 | Run tests, fix denormals, stress test silence |

---

## Useful CMake Presets (Pamplejuce built-in)

Pamplejuce ships with `CMakePresets.json`. Use these for quick builds:

```bash
# Configure + build debug
cmake --preset default
cmake --build --preset default

# Run tests
ctest --preset default
```

---

## Troubleshooting

### "AU component not found in Logic/GarageBand"
Run `auval` to validate:
```bash
auval -v aufx Lmnl AmAu
```
If it fails, check your `PLUGIN_CODE` and `PLUGIN_MANUFACTURER_CODE` in CMakeLists.txt — they must match exactly.

### "CMake can't find JUCE"
Ensure submodules initialized correctly:
```bash
git submodule status
```
If blank or missing, run:
```bash
git submodule update --init --recursive
```

### Visual Studio: "No C++ compiler found"
Open Visual Studio Installer → Modify → ensure **Desktop development with C++** is checked.

### Plugin crashes DAW on load
Almost always a real-time safety violation in `processBlock`. Check for:
- Memory allocations (new, malloc, vector push_back)
- Mutex locks
- Exceptions

Use `juce::ScopedNoDenormals` at the top of `processBlock` — this is already in `Claude.md`.

---

## Key Reference Links

- [Pamplejuce GitHub](https://github.com/sudara/pamplejuce)
- [JUCE Documentation](https://juce.com/learn/documentation)
- [JUCE DSP Module Reference](https://docs.juce.com/master/group__juce__dsp.html)
- [JUCE AudioProcessorValueTreeState](https://docs.juce.com/master/classAudioProcessorValueTreeState.html)
- [JUCE Forum](https://forum.juce.com/)
- [KVR DSP & Plugins Forum](https://www.kvraudio.com/forum/viewforum.php?f=33)

---

*LIMINAL — Ament Audio — SETUP_GUIDE.md v1.0*
