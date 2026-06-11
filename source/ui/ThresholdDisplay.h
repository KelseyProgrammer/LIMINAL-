#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Celestial threshold display.
//
// The star itself is part of the background artwork; this component sits
// exactly over it and draws only ADDITIVE, animated light: breathing glow,
// a bright hexagram that wakes with the blend level, rotating compass ticks,
// per-engine ray pulses, threshold/envelope rings and crossing ripples.
// At blend = 0 it is almost invisible so the painted card shows through.
//
// The gold threshold ring is interactive: click/drag radially to set the
// threshold parameter (wired up by the editor via the callbacks below).
class ThresholdDisplay : public juce::Component,
                         private juce::Timer
{
public:
    ThresholdDisplay();
    ~ThresholdDisplay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;

    // Called from the UI thread (timer) to update display data
    void setBlendLevel    (float blend);
    void setEnvelopeLevel (float level);
    void setThresholdValue(float threshold);

    // Knob-connected star effects
    void setSlewNorm   (float norm);    // 0 = fastest (5ms),  1 = slowest (2000ms)
    void setDepthValue (float depth);   // 0–1  → star scale + glow intensity
    void setToneValue  (float tone);    // −1…+1 → ice-blue → gold → warm-white

    void setHauntActive     (bool active);
    void setShimmerActive   (bool active);
    void setPitchGhostActive(bool active);

    // Threshold ring drag → parameter gesture (wired by PluginEditor)
    std::function<void()>      onThresholdDragStart;
    std::function<void(float)> onThresholdDrag;       // normalized 0–1
    std::function<void()>      onThresholdDragEnd;

private:
    void timerCallback() override;

    void drawEngineAxes (juce::Graphics& g, juce::Point<float> centre,
                         float outerR, float blend, float rotRad);

    float radiusToThreshold (float radius) const;
    static float thresholdToRadius (float threshold);

    // Audio-thread → UI-thread atomics
    std::atomic<float> blendLevel    { 0.f };
    std::atomic<float> envelopeLevel { 1.f };
    std::atomic<float> thresholdVal  { 0.3f };

    std::atomic<float> slewNorm  { 0.55f };  // normalized slew position
    std::atomic<float> depthVal  { 1.0f  };
    std::atomic<float> toneVal   { 0.0f  };

    std::atomic<bool> hauntActive     { false };
    std::atomic<bool> shimmerActive   { false };
    std::atomic<bool> ghostActive     { false };

    // UI-thread animation state (timer callback only)
    float displayBlend     = 0.f;
    float displayEnvelope  = 1.f;
    float displayThreshold = 0.3f;
    float displaySlew      = 0.55f;
    float displayDepth     = 1.0f;
    float displayTone      = 0.0f;

    float rotationSpeed  = 0.10f;  // degrees/frame — driven by SLEW
    float starRotation   = 0.f;    // degrees, slow drift
    float breathePhase   = 0.f;    // idle breathing oscillator
    float flashIntensity = 0.f;    // 0–1, decays each frame
    float rippleRadius   = 0.f;    // px, expands on threshold crossing
    bool  rippleActive   = false;
    float prevEnvelope   = 1.f;    // crossing detection

    bool  draggingThreshold = false;
};
