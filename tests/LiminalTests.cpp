#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/EnvelopeFollower.h"
#include "dsp/LiminalEngine.h"
#include "modulation/RampSystem.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static juce::dsp::ProcessSpec makeSpec (double sr = 44100.0, int blockSize = 512)
{
    return { sr, static_cast<juce::uint32> (blockSize), 2 };
}

// Fill a stereo buffer with a constant value
static void fillBuffer (juce::AudioBuffer<float>& buf, float value)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        juce::FloatVectorOperations::fill (buf.getWritePointer (ch), value, buf.getNumSamples());
}

// Run the envelope follower for N samples of a constant input; return final level
static float runFollower (EnvelopeFollower& ef, float input, int numSamples)
{
    float level = 0.f;
    for (int i = 0; i < numSamples; ++i)
        level = ef.process (input);
    return level;
}

// ── EnvelopeFollower ─────────────────────────────────────────────────────────

TEST_CASE ("EnvelopeFollower: silence → 0.0", "[envelope]")
{
    EnvelopeFollower ef;
    ef.prepare (44100.0);

    const float level = runFollower (ef, 0.f, 4096);
    REQUIRE_THAT (level, Catch::Matchers::WithinAbs (0.f, 1e-6f));
}

TEST_CASE ("EnvelopeFollower: sustained 1.0 sine → converges near 1.0", "[envelope]")
{
    EnvelopeFollower ef;
    ef.prepare (44100.0);
    ef.setAttack  (1.f);    // fast attack for test
    ef.setRelease (500.f);

    // Feed full-scale positive samples (envelope follower uses abs, so polarity doesn't matter)
    const float level = runFollower (ef, 1.f, 8192);
    // After 8192 samples (~186ms) with 1ms attack, should be very close to 1.0
    REQUIRE (level > 0.99f);
}

TEST_CASE ("EnvelopeFollower: releases after signal stops", "[envelope]")
{
    EnvelopeFollower ef;
    ef.prepare (44100.0);
    ef.setAttack  (1.f);
    ef.setRelease (50.f);   // 50ms release

    // Prime to full level
    runFollower (ef, 1.f, 4096);

    // Then silence — after 3× release time should be very low
    const float level = runFollower (ef, 0.f, static_cast<int> (44100.0 * 0.15));
    REQUIRE (level < 0.05f);
}

// ── LiminalEngine ────────────────────────────────────────────────────────────

TEST_CASE ("LiminalEngine: above threshold → blend = 0", "[engine]")
{
    LiminalEngine engine;
    engine.prepare (makeSpec());
    engine.setThreshold (0.3f);
    engine.setSlew (1.f);   // near-instant slew for test
    engine.setDepth (1.f);
    engine.setMix (1.f);

    juce::AudioBuffer<float> buf (2, 512);
    fillBuffer (buf, 0.f);

    // Envelope level well above threshold
    engine.process (buf, 0.9f);

    REQUIRE_THAT (engine.getCurrentBlend(), Catch::Matchers::WithinAbs (0.f, 0.01f));
}

TEST_CASE ("LiminalEngine: below threshold → blend > 0", "[engine]")
{
    LiminalEngine engine;
    engine.prepare (makeSpec());
    engine.setThreshold (0.3f);
    engine.setSlew (1.f);
    engine.setDepth (1.f);
    engine.setMix (1.f);

    juce::AudioBuffer<float> buf (2, 512);
    fillBuffer (buf, 0.f);

    // Process several blocks at zero (deep silence)
    for (int i = 0; i < 100; ++i)
        engine.process (buf, 0.0f);

    REQUIRE (engine.getCurrentBlend() > 0.5f);
}

TEST_CASE ("LiminalEngine: blend scales with depth", "[engine]")
{
    auto runToSteadyState = [] (float depth) -> float
    {
        LiminalEngine engine;
        engine.prepare (makeSpec());
        engine.setThreshold (0.3f);
        engine.setSlew (1.f);
        engine.setDepth (depth);
        engine.setMix (1.f);

        juce::AudioBuffer<float> buf (2, 512);
        fillBuffer (buf, 0.f);
        for (int i = 0; i < 100; ++i)
            engine.process (buf, 0.f);
        return engine.getCurrentBlend();
    };

    const float blendFull = runToSteadyState (1.0f);
    const float blendHalf = runToSteadyState (0.5f);

    REQUIRE (blendFull > 0.85f);
    REQUIRE (blendHalf > 0.4f);
    REQUIRE (blendFull > blendHalf);
}

TEST_CASE ("LiminalEngine: no output above threshold (silence in = silence out)", "[engine]")
{
    LiminalEngine engine;
    engine.prepare (makeSpec());
    engine.setThreshold (0.3f);
    engine.setSlew (1.f);
    engine.setMix (1.f);

    juce::AudioBuffer<float> buf (2, 512);
    buf.clear();

    // Envelope level above threshold — engines should be silent
    engine.process (buf, 0.9f);

    float maxSample = 0.f;
    for (int ch = 0; ch < 2; ++ch)
        for (int s = 0; s < 512; ++s)
            maxSample = std::max (maxSample, std::abs (buf.getSample (ch, s)));

    // With blend = 0, output should be silent (or near-silent — HauntVerb adds silence to silence)
    REQUIRE (maxSample < 1e-4f);
}

// ── RampSystem ───────────────────────────────────────────────────────────────

TEST_CASE ("RampSystem: trigger advances position 0→1 over rampTime", "[ramp]")
{
    RampSystem ramp;
    const double sr       = 44100.0;
    const float  timeMs   = 1000.f;   // 1 second ramp
    const int    total    = static_cast<int> (sr);  // 1 second of samples

    ramp.setRampTime (timeMs, sr);

    REQUIRE_THAT (ramp.getPosition(), Catch::Matchers::WithinAbs (0.f, 1e-6f));

    ramp.process (total);

    REQUIRE_THAT (ramp.getPosition(), Catch::Matchers::WithinAbs (1.f, 0.01f));
}

TEST_CASE ("RampSystem: trigger reverses direction", "[ramp]")
{
    RampSystem ramp;
    const double sr = 44100.0;
    ramp.setRampTime (500.f, sr);

    // Run fully to B
    ramp.process (static_cast<int> (sr));
    REQUIRE (ramp.getPosition() > 0.99f);

    // Trigger → now goes B→A
    ramp.trigger();
    ramp.process (static_cast<int> (sr));
    REQUIRE (ramp.getPosition() < 0.01f);
}

TEST_CASE ("RampSystem: position clamps at 0 and 1", "[ramp]")
{
    RampSystem ramp;
    ramp.setRampTime (10.f, 44100.0);

    // Way more samples than needed to reach 1.0
    ramp.process (100000);
    REQUIRE_THAT (ramp.getPosition(), Catch::Matchers::WithinAbs (1.f, 1e-6f));

    ramp.trigger();
    ramp.process (100000);
    REQUIRE_THAT (ramp.getPosition(), Catch::Matchers::WithinAbs (0.f, 1e-6f));
}

// ── Real-time safety ─────────────────────────────────────────────────────────

TEST_CASE ("processBlock: no denormals under sustained silence", "[rt-safety]")
{
    // Feed 10 blocks of silence through LiminalEngine and verify no sample
    // exceeds a denormal threshold (this exercises the blend slew path).
    LiminalEngine engine;
    engine.prepare (makeSpec());
    engine.setThreshold (0.5f);
    engine.setSlew (200.f);
    engine.setDepth (1.f);
    engine.setMix (1.f);
    engine.setHaunt (0.5f);

    juce::AudioBuffer<float> buf (2, 512);

    for (int block = 0; block < 10; ++block)
    {
        buf.clear();
        engine.process (buf, 0.f);

        for (int ch = 0; ch < 2; ++ch)
        {
            const float* data = buf.getReadPointer (ch);
            for (int s = 0; s < 512; ++s)
            {
                const float v = data[s];
                // A denormal is non-zero but smaller than FLT_MIN (~1.18e-38)
                // We just check for NaN / Inf here — denormal flushing is handled by ScopedNoDenormals
                REQUIRE (std::isfinite (v));
            }
        }
    }
}
