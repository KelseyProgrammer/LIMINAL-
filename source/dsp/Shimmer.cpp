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

    // Distance-based Hann windows — sum to 1 everywhere (50% overlap property).
    // dist = how far the read head is BEHIND the write head.
    // When dist occupies [0, kGrainSamples], phase sweeps 0→1, giving the full window shape.
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

void Shimmer::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    for (int ch = 0; ch < 2; ++ch)
    {
        voice1[ch].prepare (spec);
        voice2[ch].prepare (spec);
    }

    feedbackBuffer.setSize (static_cast<int> (spec.numChannels),
                            static_cast<int> (spec.maximumBlockSize));
    feedbackBuffer.clear();

    frozenBuffer.setSize (static_cast<int> (spec.numChannels),
                          static_cast<int> (spec.maximumBlockSize));
    frozenBuffer.clear();

    setInterval (intervalSemitones);
}

void Shimmer::process (juce::AudioBuffer<float>& buffer, float blendFactor)
{
    if (blendFactor <= 0.f && crystallizeAmount <= 0.f)
        return;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = std::min (buffer.getNumChannels(), 2);

    // Decide whether to latch (freeze) shimmer output
    const bool shouldFreeze = (crystallizeAmount > 0.9f);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet  = buffer.getWritePointer (ch);
        auto* fb   = feedbackBuffer.getWritePointer (ch);
        auto* frz  = frozenBuffer.getWritePointer (ch);

        for (int s = 0; s < numSamples; ++s)
        {
            const float dryIn = wet[s];

            // Input to pitch shifters = dry + gentle feedback
            const float shifterIn = dryIn + fb[s] * feedbackLevel;

            // Voice 1: user interval. Voice 2: a fifth (7st) above voice 1.
            // Using a fifth instead of an octave gives a richer, less harsh chord.
            const float v1Out = voice1[ch].processSample (shifterIn, ch);
            const float v2Out = voice2[ch].processSample (shifterIn, ch);

            // Balance: voice 1 slightly louder, voice 2 as a gentler harmonic layer
            const float shimmerRaw = v1Out * 0.65f + v2Out * 0.35f;

            // One-pole LP to smooth grain-boundary artifacts
            // Coefficient 0.25 = ~4kHz cutoff at 44.1kHz — removes the "grit"
            lpState[ch] = 0.25f * shimmerRaw + 0.75f * lpState[ch];
            const float shimmerOut = lpState[ch];

            // Update feedback — clamp and attenuate to prevent runaway buildup
            fb[s] = juce::jlimit (-1.f, 1.f, shimmerOut);

            // Crystallize: lerp between decaying and frozen output
            if (shouldFreeze)
                frz[s] = shimmerOut;

            const float wetOut = shimmerOut * (1.f - crystallizeAmount)
                               + frz[s]     * crystallizeAmount;

            // Wet/dry crossfade
            wet[s] = dryIn * (1.f - blendFactor) + wetOut * blendFactor;
        }
    }
}

void Shimmer::setCrystallize (float amount)
{
    crystallizeAmount = juce::jlimit (0.f, 1.f, amount);
}

void Shimmer::setInterval (int semitones)
{
    intervalSemitones = semitones;
    const float ratio = semitonesToRatio (semitones);
    for (int ch = 0; ch < 2; ++ch)
    {
        voice1[ch].setPitchRatio (ratio);
        // Voice 2: a perfect fifth (7 semitones) above voice 1.
        // Previously was an octave (+12) which produced very harsh two-octave stacking.
        // A fifth gives a richer, more musical shimmer chord.
        voice2[ch].setPitchRatio (semitonesToRatio (semitones + 7));
    }
}

void Shimmer::setFeedback (float feedback)
{
    // Cap at 0.6 to prevent runaway feedback buildup that creates harsh resonances
    feedbackLevel = juce::jlimit (0.f, 0.6f, feedback);
}

float Shimmer::semitonesToRatio (int semitones)
{
    return std::pow (2.f, static_cast<float> (semitones) / 12.f);
}
