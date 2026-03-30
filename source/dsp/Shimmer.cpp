#include "Shimmer.h"

#include <cmath>

//==============================================================================
// PitchShifterVoice — simple overlap-add pitch shifter (two grains at 180° offset)
//==============================================================================

void PitchShifterVoice::prepare (const juce::dsp::ProcessSpec& spec)
{
    grainBuffer.setSize (static_cast<int> (spec.numChannels), kGrainSamples * 2);
    grainBuffer.clear();
    writePos    = 0;
    readPos     = 0.f;
    windowPhase = 0.f;
}

float PitchShifterVoice::processSample (float input, int /*channel*/)
{
    // Write input into circular grain buffer
    const int bufLen = grainBuffer.getNumSamples();
    auto* buf = grainBuffer.getWritePointer (0);
    buf[writePos] = input;
    writePos = (writePos + 1) % bufLen;

    // Grain 1 (phase 0)
    const float readIdx1 = readPos;
    const int   ri1a     = static_cast<int> (readIdx1) % bufLen;
    const int   ri1b     = (ri1a + 1) % bufLen;
    const float frac1    = readIdx1 - static_cast<float> (static_cast<int> (readIdx1));
    const float grain1   = buf[ri1a] * (1.f - frac1) + buf[ri1b] * frac1;

    // Grain 2 (half-grain offset)
    const float readIdx2 = std::fmod (readPos + kGrainSamples * 0.5f, static_cast<float> (bufLen));
    const int   ri2a     = static_cast<int> (readIdx2) % bufLen;
    const int   ri2b     = (ri2a + 1) % bufLen;
    const float frac2    = readIdx2 - static_cast<float> (static_cast<int> (readIdx2));
    const float grain2   = buf[ri2a] * (1.f - frac2) + buf[ri2b] * frac2;

    // Hann window for each grain (based on read position within grain)
    const float phase1 = std::fmod (readPos / static_cast<float> (kGrainSamples), 1.f);
    const float phase2 = std::fmod ((readPos + kGrainSamples * 0.5f) / static_cast<float> (kGrainSamples), 1.f);
    const float win1   = 0.5f - 0.5f * std::cos (2.f * juce::MathConstants<float>::pi * phase1);
    const float win2   = 0.5f - 0.5f * std::cos (2.f * juce::MathConstants<float>::pi * phase2);

    // Advance read pointer by pitch ratio
    readPos = std::fmod (readPos + pitchRatio, static_cast<float> (bufLen));

    return grain1 * win1 + grain2 * win2;
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

            // Input to pitch shifters = dry + feedback
            const float shifterIn = dryIn + fb[s] * feedbackLevel;

            // Voice 1: user interval
            const float v1Out = voice1[ch].processSample (shifterIn, ch);

            // Voice 2: one octave above voice 1
            const float v2Out = voice2[ch].processSample (v1Out, ch);

            const float shimmerOut = (v1Out + v2Out) * 0.5f;

            // Update feedback
            fb[s] = shimmerOut;

            // Crystallize: lerp between decaying and frozen output
            if (shouldFreeze)
                frz[s] = shimmerOut;

            const float wetOut = shimmerOut * (1.f - crystallizeAmount)
                               + frz[s]     * crystallizeAmount;

            // Blend onto output
            wet[s] = dryIn + wetOut * blendFactor;
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
        voice2[ch].setPitchRatio (semitonesToRatio (semitones + 12)); // octave above voice1
    }
}

void Shimmer::setFeedback (float feedback)
{
    feedbackLevel = juce::jlimit (0.f, 0.95f, feedback);
}

float Shimmer::semitonesToRatio (int semitones)
{
    return std::pow (2.f, static_cast<float> (semitones) / 12.f);
}
