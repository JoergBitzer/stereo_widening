/**
 * @file CorrelationMeterComponent.h
 * @brief Horizontal -1 .. +1 correlation bar, the usual "phase meter" convention:
 *        +1 = mono, 0 = decorrelated / wide, -1 = out of phase (mono sum cancels).
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include "MeterComponentBase.h"
#include "StereoMeterState.h"

class CorrelationMeterComponent : public MeterComponentBase
{
public:
    explicit CorrelationMeterComponent(StereoMeterState& stateToDisplay);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void refresh() override; // smooths displayedValue towards state.getCorrelation()

    StereoMeterState& state;
    float displayedValue = 0.0f; // smoothed for a less jittery needle

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CorrelationMeterComponent)
};
