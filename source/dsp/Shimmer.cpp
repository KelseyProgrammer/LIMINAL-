#include "Shimmer.h"

#include <cmath>

//==============================================================================
// PitchShifterVoice — simple overlap-add pitch shifter (two grains at 180° offset)
//==============================================================================

void PitchShifterVoice::prepare (const juce::dsp::ProcessSpec& spec)
{
    juce::ignoreUnused (spec);
    const int bufLen = kGrainSamples * 2;
    grainBuffer.setSize (1, bufLen);
    grainBuffer.clear();
    // Start write head at kGrainSamples so readPos=0 begins exactly one grain behind.
    // This prevents the read head from overtaking the write head and reading unwritten data,
    // which was the primary cause of metallic "grinding" artifacts.
    writePos = kGrainSamples;
    readPos  = 0.f;
}

float PitchShifterVoice::processSample (float input, int /*channel*/)
{
    const int   bufLen  = grainBuffer.getNumSamples();  // kGrainSamples * 2
    const float fBufLen = static_cast<float> (bufLen);
    const float fGrain  = static_cast<float> (kGrainSamples);
    auto* buf = grainBuffer.getWritePointer (0);

    // Write input into circular buffer
    buf[writePos] = input;

    // Grain 2 is half a grain ahead of grain 1 (50% overlap for constant-power crossfade)
    const float readPos2 = std::fmod (readPos + fGrain * 0.5f, fBufLen);

    // Linear-interpolated reads
    auto readInterp = [&](float rp) -> float
    {
        const int   ia   = static_cast<int> (rp) & (bufLen - 1);
        const int   ib   = (ia + 1) & (bufLen - 1);
        const float frac = rp - static_cast<float> (static_cast<int> (rp));
        return buf[ia] * (1.f - frac) + buf[ib] * frac;
    };

    const float g1 = readInterp (readPos);
    const float g2 = readInterp (readPos2);

    // Distance-based Hann windows — sum to 1 everywhere (50% overlap property)
    auto computeWindow = [&](float rp) -> float
    {
        const float dist  = std::fmod (static_cast<float> (writePos) - rp + fBufLen, fBufLen);
        const float phase = std::fmod (dist, fGrain) / fGrain;
        return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);
    };

    const float win1 = computeWindow (readPos);
    const float win2 = computeWindow (readPos2);

    // Advance read and write positions
    readPos  = std::fmod (readPos + pitchRatio, fBufLen);
    writePos = (writePos + 1) & (bufLen - 1);

    return g1 * win1 + g2 * win2;
}

//==============================================================================
// Shimmer
//==============================================================================

static constexpr float kCascadeDelayMs   = 340.f;
static constexpr float kCascadeDampLP    = 0.30f;   // LP in the feedback path
static constexpr float kFreezeTriggerLvl = 0.08f;   // crystallize level that latches
static constexpr float kDetuneCents      = 3.f;     // L/R width detune

void Shimmer::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    for (int ch = 0; ch < 2; ++ch)
    {
        voice1[ch].prepare (spec);
        voice2[ch].prepare (spec);
        cascadeDamp[ch] = 0.f;
        lpState[ch]     = 0.f;
    }

    const int cascadeSamples = static_cast<int> (
        std::ceil (kCascadeDelayMs / 1000.0 * sampleRate)) + 2;
    cascadeDelay.prepare (spec);
    cascadeDelay.setMaximumDelayInSamples (cascadeSamples);
    cascadeDelay.setDelay (static_cast<float> (kCascadeDelayMs / 1000.0 * sampleRate));

    freezeRing.setSize (2, kFreezeRingSize);
    freezeRing.clear();
    freezeWritePos = 0;

    frozenLoop.setSize (2, kFreezeRingSize);
    frozenLoop.clear();
    frozenLength  = 0;
    frozenReadPos = 0.f;
    frozenEnv     = 0.f;
    frozenActive  = false;

    setInterval (intervalSemitones);
}

void Shimmer::captureFreeze()
{
    // Latch the most recent ~2s of shimmer output from the ring
    frozenLength = juce::jmin (kFreezeRingSize,
                               static_cast<int> (sampleRate * 2.0));
    const int startPos = (freezeWritePos - frozenLength + kFreezeRingSize)
                         & (kFreezeRingSize - 1);

    for (int ch = 0; ch < 2; ++ch)
    {
        const auto* src = freezeRing.getReadPointer (ch);
        auto*       dst = frozenLoop.getWritePointer (ch);
        for (int i = 0; i < frozenLength; ++i)
            dst[i] = src[(startPos + i) & (kFreezeRingSize - 1)];
    }

    frozenReadPos = 0.f;
    frozenEnv     = 1.f;
    frozenActive  = true;
}

void Shimmer::renderWet (const juce::AudioBuffer<float>& in,
                         juce::AudioBuffer<float>& wet,
                         float blendFactor)
{
    const int numSamples  = in.getNumSamples();
    const int numChannels = juce::jmin (in.getNumChannels(), wet.getNumChannels(), 2);

    wet.clear();

    // Freeze latches when crystallize rises past the trigger level
    if (crystallizeAmount >= kFreezeTriggerLvl && prevCrystallize < kFreezeTriggerLvl)
        captureFreeze();
    if (crystallizeAmount < kFreezeTriggerLvl * 0.5f)
        frozenActive = false;
    prevCrystallize = crystallizeAmount;

    const bool liveActive = blendFactor > 0.f;
    if (! liveActive && ! frozenActive)
        return;

    // Frozen layer decay: 1.0 = infinite hold, below = decays over 0.5–8.5s
    float frozenDecayCoeff = 1.f;
    if (crystallizeAmount < 0.999f)
    {
        const double decaySeconds = 0.5 + 8.0 * crystallizeAmount;
        frozenDecayCoeff = static_cast<float> (
            std::exp (-1.0 / (sampleRate * decaySeconds)));
    }

    for (int s = 0; s < numSamples; ++s)
    {
        // ── Frozen loop sample (advance once per frame, shared across channels) ──
        float frozenL = 0.f, frozenR = 0.f;
        if (frozenActive && frozenLength > 256)
        {
            // Boundary crossfade window (same trick as PitchGhost loop)
            const float fadeZone  = juce::jmin (1024.f, frozenLength * 0.05f);
            const float distToEnd = static_cast<float> (frozenLength) - frozenReadPos;
            const float loopGain  = juce::jmin (1.f,
                                     juce::jmin (frozenReadPos, distToEnd) / fadeZone);

            const int   i0   = static_cast<int> (frozenReadPos) % frozenLength;
            const int   i1   = (i0 + 1) % frozenLength;
            const float frac = frozenReadPos - std::floor (frozenReadPos);

            const auto* fl = frozenLoop.getReadPointer (0);
            const auto* fr = frozenLoop.getReadPointer (juce::jmin (1, frozenLoop.getNumChannels() - 1));
            frozenL = (fl[i0] * (1.f - frac) + fl[i1] * frac) * loopGain;
            frozenR = (fr[i0] * (1.f - frac) + fr[i1] * frac) * loopGain;

            frozenReadPos += 1.f;
            if (frozenReadPos >= static_cast<float> (frozenLength))
                frozenReadPos = 0.f;

            frozenEnv *= frozenDecayCoeff;
            if (frozenEnv < 1e-5f)
                frozenActive = false;
        }

        const int ringPos = freezeWritePos & (kFreezeRingSize - 1);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float dryIn = in.getReadPointer (ch)[s];

            // ── Shimmer cascade ──────────────────────────────────────────────
            // Feedback comes back ~340ms later and gets shifted again, so the
            // texture climbs the interval on every pass.
            const float fbSample  = cascadeDelay.popSample (ch);
            const float shifterIn = dryIn * blendFactor + fbSample * feedbackLevel;

            const float v1 = voice1[ch].processSample (shifterIn, ch);
            const float v2 = voice2[ch].processSample (shifterIn, ch);
            const float raw = v1 * 0.65f + v2 * 0.35f;

            // One-pole LP smooths grain-boundary artifacts
            lpState[ch] = 0.25f * raw + 0.75f * lpState[ch];
            const float live = lpState[ch];

            // Damped, clamped feedback into the cascade delay
            cascadeDamp[ch] = kCascadeDampLP * live
                            + (1.f - kCascadeDampLP) * cascadeDamp[ch];
            cascadeDelay.pushSample (ch, juce::jlimit (-1.f, 1.f, cascadeDamp[ch]));

            // Record live shimmer into the freeze ring
            freezeRing.getWritePointer (ch)[ringPos] = live;

            // ── Output ───────────────────────────────────────────────────────
            // Live layer rides the blend; the frozen drone deliberately does
            // not — a latched crystal sustains even while you play.
            const float frozen = (ch == 0 ? frozenL : frozenR) * frozenEnv;
            wet.getWritePointer (ch)[s] =
                  live * (1.f - crystallizeAmount * 0.3f)
                + frozen * crystallizeAmount;
        }

        ++freezeWritePos;
    }
}

void Shimmer::setCrystallize (float amount)
{
    crystallizeAmount = juce::jlimit (0.f, 1.f, amount);
}

void Shimmer::setInterval (int semitones)
{
    intervalSemitones = semitones;
    const float detune = kDetuneCents / 100.f;  // cents → semitones

    for (int ch = 0; ch < 2; ++ch)
    {
        const float chDetune = (ch == 0 ? detune : -detune);
        voice1[ch].setPitchRatio (semitonesToRatio (static_cast<float> (semitones) + chDetune));
        // Voice 2: a perfect fifth above voice 1 for a richer shimmer chord
        voice2[ch].setPitchRatio (semitonesToRatio (static_cast<float> (semitones) + 7.f - chDetune));
    }
}

void Shimmer::setFeedback (float feedback)
{
    // The cascade loop is damped and clamped, so it tolerates more feedback
    // than the old instant loop — still capped well below unity.
    feedbackLevel = juce::jlimit (0.f, 0.75f, feedback);
}

float Shimmer::semitonesToRatio (float semitones)
{
    return std::pow (2.f, semitones / 12.f);
}
