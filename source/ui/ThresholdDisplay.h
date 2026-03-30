#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Celestial threshold display — compass rose star that breathes with blend,
// pulses per-engine, and flashes when the threshold is crossed.
class ThresholdDisplay : public juce::Component,
                         private juce::Timer
{
public:
    ThresholdDisplay();
    ~ThresholdDisplay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Called from the UI thread (timer) to update display data
    void setBlendLevel    (float blend);
    void setEnvelopeLevel (float level);
    void setThresholdValue(float threshold);

    void setHauntActive     (bool active);
    void setShimmerActive   (bool active);
    void setPitchGhostActive(bool active);

private:
    void timerCallback() override;

    void drawRotatedStar (juce::Graphics& g, juce::Point<float> centre,
                          float innerR, float outerR, float alpha, float rotRad);
    void drawEngineAxes  (juce::Graphics& g, juce::Point<float> centre,
                          float outerR, float blend, float rotRad);

    // Audio-thread → UI-thread atomics
    std::atomic<float> blendLevel    { 0.f };
    std::atomic<float> envelopeLevel { 1.f };
    std::atomic<float> thresholdVal  { 0.3f };

    std::atomic<bool> hauntActive     { false };
    std::atomic<bool> shimmerActive   { false };
    std::atomic<bool> ghostActive     { false };

    // UI-thread animation state (timer callback only)
    float displayBlend     = 0.f;
    float displayEnvelope  = 1.f;
    float displayThreshold = 0.3f;

    float starRotation   = 0.f;    // degrees, slow drift
    float flashIntensity = 0.f;    // 0–1, decays each frame
    float rippleRadius   = 0.f;    // px, expands on threshold crossing
    bool  rippleActive   = false;
    float prevEnvelope   = 1.f;    // crossing detection
};
