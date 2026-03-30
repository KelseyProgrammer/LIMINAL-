#include "RampSystem.h"

#include <algorithm>

void RampSystem::setRampTime (float timeMs, double sampleRate)
{
    if (timeMs <= 0.f || sampleRate <= 0.0)
    {
        increment = 1.f;
        return;
    }
    const double totalSamples = sampleRate * (static_cast<double> (timeMs) / 1000.0);
    increment = static_cast<float> (1.0 / totalSamples);
}

void RampSystem::trigger()
{
    goingToB = !goingToB;
}

void RampSystem::process (int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        if (goingToB)
            position = std::min (1.f, position + increment);
        else
            position = std::max (0.f, position - increment);
    }
}
