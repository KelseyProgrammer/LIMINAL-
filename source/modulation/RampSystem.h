#pragma once

class RampSystem
{
public:
    void  setRampTime (float timeMs, double sampleRate);
    void  trigger();                          // toggles direction A→B or B→A
    float getPosition() const { return position; }  // 0.0 = A, 1.0 = B
    void  process (int numSamples);

private:
    float position  = 0.f;
    float increment = 0.f;
    bool  goingToB  = true;  // true = A→B, false = B→A
};
