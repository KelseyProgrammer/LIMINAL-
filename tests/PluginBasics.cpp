#include "helpers/test_helpers.h"
#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

TEST_CASE ("one is equal to one", "[dummy]")
{
    REQUIRE (1 == 1);
}

TEST_CASE ("Plugin instance", "[instance]")
{
    PluginProcessor testPlugin;

    SECTION ("name")
    {
        CHECK_THAT (testPlugin.getName().toStdString(),
            Catch::Matchers::Equals ("LIMINAL"));
    }
}


// ── Snapshot interpolation ────────────────────────────────────────────────────
// Note: getRawParameterValue atomics are written directly because APVTS
// parameter listeners require a message loop to propagate setValue() calls.

static void setRaw (juce::AudioProcessorValueTreeState& apvts,
                    const juce::String& id, float rawValue)
{
    *apvts.getRawParameterValue (id) = rawValue;
}

TEST_CASE ("Snapshot interpolation: ramp at 0.0 uses snapshot A depth", "[ramp][snapshot]")
{
    PluginProcessor p;
    p.prepareToPlay (44100.0, 512);

    // Snapshot A: depth=0, threshold=0.8, fast slew
    setRaw (p.apvts, "depth",     0.f);
    setRaw (p.apvts, "threshold", 0.8f);
    setRaw (p.apvts, "slew",      5.f);   // 5ms — fastest slew in range
    p.captureSnapshotA();

    // Snapshot B: depth=1 (everything else same)
    setRaw (p.apvts, "depth", 1.f);
    p.captureSnapshotB();

    // Position = 0 → snapshot A (depth=0) → blend stays near 0 in silence
    p.rampPosition.store (0.f);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    buf.clear();
    for (int i = 0; i < 20; ++i)
        p.processBlock (buf, midi);

    REQUIRE_THAT (p.getBlendLevel(), Catch::Matchers::WithinAbs (0.f, 0.05f));
}

TEST_CASE ("Snapshot interpolation: ramp at 1.0 uses snapshot B depth", "[ramp][snapshot]")
{
    PluginProcessor p;
    p.prepareToPlay (44100.0, 512);

    setRaw (p.apvts, "depth",     0.f);
    setRaw (p.apvts, "threshold", 0.8f);
    setRaw (p.apvts, "slew",      5.f);
    p.captureSnapshotA();

    setRaw (p.apvts, "depth", 1.f);
    p.captureSnapshotB();

    // Position = 1 → snapshot B (depth=1) → blend approaches 1 in silence
    p.rampPosition.store (1.f);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    buf.clear();
    for (int i = 0; i < 20; ++i)
        p.processBlock (buf, midi);

    REQUIRE (p.getBlendLevel() > 0.7f);
}

TEST_CASE ("Snapshot interpolation: ramp at 0.5 gives intermediate blend", "[ramp][snapshot]")
{
    PluginProcessor p;
    p.prepareToPlay (44100.0, 512);

    setRaw (p.apvts, "depth",     0.f);
    setRaw (p.apvts, "threshold", 0.8f);
    setRaw (p.apvts, "slew",      5.f);
    p.captureSnapshotA();

    setRaw (p.apvts, "depth", 1.f);
    p.captureSnapshotB();

    // Position = 0.5 → depth interpolates to 0.5 → blend approaches 0.5
    p.rampPosition.store (0.5f);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    buf.clear();
    for (int i = 0; i < 20; ++i)
        p.processBlock (buf, midi);

    const float blend = p.getBlendLevel();
    REQUIRE (blend > 0.1f);
    REQUIRE (blend < 0.9f);
}

#ifdef PAMPLEJUCE_IPP
    #include <ipp.h>

TEST_CASE ("IPP version", "[ipp]")
{
    #if defined(__APPLE__)
        // macOS uses 2021.9.1 from pip wheel (only x86_64 version available)
        CHECK_THAT (ippsGetLibVersion()->Version, Catch::Matchers::Equals ("2021.9.1 (r0x7e208212)"));
    #else
        CHECK_THAT (ippsGetLibVersion()->Version, Catch::Matchers::Equals ("2022.3.0 (r0x0fc08bb1)"));
    #endif
}
#endif
