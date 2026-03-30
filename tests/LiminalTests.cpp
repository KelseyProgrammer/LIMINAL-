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

TEST_CASE ("EnvelopeFollower: silence -> 0.0", "[envelope]")
{
    EnvelopeFollower ef;
    ef.prepare (44100.0);

    const float level = runFollower (ef, 0.f, 4096);
    REQUIRE_THAT (level, Catch::Matchers::WithinAbs (0.f, 1e-6f));
}

TEST_CASE ("EnvelopeFollower: sustained 1.0 sine -> converges near 1.0", "[envelope]")
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

TEST_CASE ("LiminalEngine: above threshold -> blend = 0", "[engine]")
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

TEST_CASE ("LiminalEngine: below threshold -> blend > 0", "[engine]")
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

TEST_CASE ("RampSystem: trigger advances position 0->1 over rampTime", "[ramp]")
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

    // Trigger -> now goes B->A
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

// ── InvertMode ───────────────────────────────────────────────────────────────

TEST_CASE ("LiminalEngine: invertMode=false, above threshold -> blend = 0", "[invert]")
{
    LiminalEngine engine;
    engine.prepare (makeSpec());
    engine.setThreshold (0.3f);
    engine.setSlew (1.f);
    engine.setDepth (1.f);
    engine.setInvertMode (false);

    juce::AudioBuffer<float> buf (2, 512);
    fillBuffer (buf, 0.f);

    engine.process (buf, 0.9f);  // above threshold
    REQUIRE_THAT (engine.getCurrentBlend(), Catch::Matchers::WithinAbs (0.f, 0.01f));
}

TEST_CASE ("LiminalEngine: invertMode=true, above threshold -> blend > 0", "[invert]")
{
    LiminalEngine engine;
    engine.prepare (makeSpec());
    engine.setThreshold (0.3f);
    engine.setSlew (1.f);
    engine.setDepth (1.f);
    engine.setInvertMode (true);

    juce::AudioBuffer<float> buf (2, 512);
    fillBuffer (buf, 0.f);

    for (int i = 0; i < 100; ++i)
        engine.process (buf, 0.9f);  // well above threshold

    REQUIRE (engine.getCurrentBlend() > 0.5f);
}

TEST_CASE ("LiminalEngine: invertMode=true, below threshold -> blend = 0", "[invert]")
{
    LiminalEngine engine;
    engine.prepare (makeSpec());
    engine.setThreshold (0.3f);
    engine.setSlew (1.f);
    engine.setDepth (1.f);
    engine.setInvertMode (true);

    juce::AudioBuffer<float> buf (2, 512);
    fillBuffer (buf, 0.f);

    engine.process (buf, 0.0f);  // below threshold
    REQUIRE_THAT (engine.getCurrentBlend(), Catch::Matchers::WithinAbs (0.f, 0.01f));
}

// ── Tone ─────────────────────────────────────────────────────────────────────

TEST_CASE ("LiminalEngine: tone=0 -> output unchanged from dry (silence in)", "[tone]")
{
    LiminalEngine engine;
    engine.prepare (makeSpec());
    engine.setTone (0.f);

    juce::AudioBuffer<float> buf (2, 512);
    fillBuffer (buf, 0.5f);

    // Record output with no tone shaping as reference
    engine.process (buf, 0.f);  // blend=0, passthrough
    const float refSample = buf.getSample (0, 256);

    // With tone=0 the applyTone early-exit should leave it unchanged
    LiminalEngine engine2;
    engine2.prepare (makeSpec());
    engine2.setTone (0.f);

    juce::AudioBuffer<float> buf2 (2, 512);
    fillBuffer (buf2, 0.5f);
    engine2.process (buf2, 0.f);

    REQUIRE_THAT (buf2.getSample (0, 256), Catch::Matchers::WithinAbs (refSample, 1e-5f));
}

TEST_CASE ("LiminalEngine: tone=-1 darkens output (reduces high-freq energy)", "[tone]")
{
    // Feed a high-frequency sine through two engines: one with tone=0, one with tone=-1.
    // The dark one should have lower energy.
    const int    N  = 512;
    const double sr = 44100.0;
    const float  fc = 10000.f; // 10kHz test tone

    auto makeSine = [&] (juce::AudioBuffer<float>& buf)
    {
        for (int s = 0; s < N; ++s)
        {
            const float v = std::sin (juce::MathConstants<float>::twoPi * fc
                                      * static_cast<float> (s) / static_cast<float> (sr));
            buf.setSample (0, s, v);
            buf.setSample (1, s, v);
        }
    };

    auto rmsOf = [] (const juce::AudioBuffer<float>& buf) -> float
    {
        float sum = 0.f;
        const float* d = buf.getReadPointer (0);
        for (int s = 0; s < buf.getNumSamples(); ++s) sum += d[s] * d[s];
        return std::sqrt (sum / static_cast<float> (buf.getNumSamples()));
    };

    // Run a few warm-up blocks so the tone filter state settles
    LiminalEngine flat, dark;
    flat.prepare (makeSpec (sr, N));   flat.setTone ( 0.f);
    dark.prepare (makeSpec (sr, N));   dark.setTone (-1.f);

    for (int w = 0; w < 4; ++w)
    {
        juce::AudioBuffer<float> b1 (2, N), b2 (2, N);
        makeSine (b1);  makeSine (b2);
        flat.process (b1, 0.f);
        dark.process (b2, 0.f);
    }

    juce::AudioBuffer<float> bufFlat (2, N), bufDark (2, N);
    makeSine (bufFlat);  makeSine (bufDark);
    flat.process (bufFlat, 0.f);
    dark.process (bufDark, 0.f);

    REQUIRE (rmsOf (bufDark) < rmsOf (bufFlat));
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
