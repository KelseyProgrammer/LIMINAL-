#include "EnvelopeFollower.h"

#include <cmath>

void EnvelopeFollower::prepare (double sr)
{
    sampleRate = sr;
    setAttack  (5.f);    // 5 ms default
    setRelease (200.f);  // 200 ms default
    envelope = 0.f;
}

float EnvelopeFollower::process (float inputSample)
{
    const float absInput = std::abs (inputSample);
    const float coeff = (absInput > envelope) ? attackCoeff : releaseCoeff;
    envelope = absInput + coeff * (envelope - absInput);
    return envelope;
}

void EnvelopeFollower::setAttack (float attackMs)
{
    attackCoeff = msToCoeff (attackMs, sampleRate);
}

void EnvelopeFollower::setRelease (float releaseMs)
{
    releaseCoeff = msToCoeff (releaseMs, sampleRate);
}

float EnvelopeFollower::msToCoeff (float ms, double sr)
{
    if (ms <= 0.f) return 0.f;
    return static_cast<float> (std::exp (-1.0 / (sr * (static_cast<double> (ms) / 1000.0))));
}
