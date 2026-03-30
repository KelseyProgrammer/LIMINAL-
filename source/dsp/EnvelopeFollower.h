#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class EnvelopeFollower
{
public:
    void prepare (double sampleRate);

    // Call per sample; returns current envelope level 0.0–1.0
    float process (float inputSample);

    float getCurrentLevel() const { return envelope; }

    void setAttack  (float attackMs);
    void setRelease (float releaseMs);

private:
    float envelope       = 0.f;
    float attackCoeff    = 0.f;
    float releaseCoeff   = 0.f;
    double sampleRate    = 44100.0;

    static float msToCoeff (float ms, double sr);
};
