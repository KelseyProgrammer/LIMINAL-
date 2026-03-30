#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Celestial threshold display — a living geometric star whose axes pulse
// per-engine and scale with the LiminalEngine blend factor.
class ThresholdDisplay : public juce::Component,
                         private juce::Timer
{
public:
    ThresholdDisplay();
    ~ThresholdDisplay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Called from the UI thread (timer) to update display data
    void setBlendLevel    (float blend);        // 0.0–1.0 from LiminalEngine
    void setEnvelopeLevel (float level);        // 0.0–1.0 from EnvelopeFollower
    void setThresholdValue(float threshold);    // 0.0–1.0 parameter value

    void setHauntActive     (bool active);
    void setShimmerActive   (bool active);
    void setPitchGhostActive(bool active);

private:
    void timerCallback() override;
    void drawStar (juce::Graphics& g, juce::Point<float> centre,
                   float innerR, float outerR, float blendLevel);

    std::atomic<float> blendLevel    { 0.f };
    std::atomic<float> envelopeLevel { 1.f };
    std::atomic<float> thresholdVal  { 0.3f };

    std::atomic<bool> hauntActive     { false };
    std::atomic<bool> shimmerActive   { false };
    std::atomic<bool> ghostActive     { false };

    float displayBlend     = 0.f;
    float displayEnvelope  = 1.f;
    float displayThreshold = 0.3f;
};
