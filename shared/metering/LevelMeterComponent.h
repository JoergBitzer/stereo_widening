/**
 * @file LevelMeterComponent.h
 * @brief Four vertical bars (L, R, M, S) showing RMS (solid) and peak (line), plus a
 *        text readout of the width estimate S-M in dB (0 dB = M and S equal power).
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

#include "StereoMeterState.h"

class LevelMeterComponent : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeterComponent(StereoMeterState& stateToDisplay);
    ~LevelMeterComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setDbRange(float minDb, float maxDb) { rangeMinDb = minDb; rangeMaxDb = maxDb; }

private:
    void timerCallback() override;
    void drawBar(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& name,
                 float rmsDb, float peakDb) const;
    float dbToFraction(float db) const noexcept;

    StereoMeterState& state;
    float rangeMinDb = -60.0f;
    float rangeMaxDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeterComponent)
};
