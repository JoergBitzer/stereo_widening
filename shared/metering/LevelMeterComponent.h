/**
 * @file LevelMeterComponent.h
 * @brief Four vertical bars (L, R, M, S) showing RMS (green/amber/red zones) and peak
 *        (white line + numeric readout), a shared dB scale to the right, and a text
 *        readout of the width estimate S-M in dB (0 dB = M and S equal power).
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include "MeterComponentBase.h"
#include "StereoMeterState.h"

class LevelMeterComponent : public MeterComponentBase
{
public:
    explicit LevelMeterComponent(StereoMeterState& stateToDisplay, juce::String labelText = {});

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setDbRange(float minDb, float maxDb) { rangeMinDb = minDb; rangeMaxDb = maxDb; }

private:
    // refresh() is not overridden: paint() reads state's atomics directly, so there is
    // nothing to precompute per tick (unlike the goniometer's FIFO drain or the
    // correlation meter's display smoothing)
    void drawScale(juce::Graphics& g, juce::Rectangle<float> barsBounds, juce::Rectangle<float> scaleBounds) const;
    void drawBar(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& name,
                 float rmsDb, float peakDb) const;
    float dbToFraction(float db) const noexcept;

    StereoMeterState& state;
    juce::String label;
    float rangeMinDb = -60.0f;
    float rangeMaxDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeterComponent)
};
