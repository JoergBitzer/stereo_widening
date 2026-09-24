/**
 * @file GoniometerComponent.h
 * @brief Lissajous / vectorscope display of the stereo image.
 *
 * Plots S (= (L-R)/2) on the horizontal axis and M (= (L+R)/2) on the vertical axis, the
 * usual goniometer convention: a mono signal draws a vertical line, a signal panned hard
 * left or right draws a diagonal at +-45 degrees, and a wide/decorrelated signal fills a
 * circle. This matches the plotting convention in python/stereo_eval/report.py, so a
 * Python plot and this display can be compared directly.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <deque>

#include "MeterComponentBase.h"
#include "StereoMeterState.h"

class GoniometerComponent : public MeterComponentBase
{
public:
    explicit GoniometerComponent(StereoMeterState& stateToDisplay, juce::String labelText = {});

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** How many of the most recently received points stay visible ("persistence"). */
    void setHistoryLength(int numPoints) { maxHistoryPoints = juce::jmax(1, numPoints); }

private:
    void refresh() override; // drains the FIFO into history
    juce::Point<float> toScreen(float s, float m) const;

    StereoMeterState& state;
    juce::String label;

    std::deque<juce::Point<float>> history; // in normalised (-1..1, -1..1) S/M coordinates
    int maxHistoryPoints = 6000; // ~130 ms of points at 48 kHz, redrawn every timer tick

    std::vector<float> drainX, drainY; // reused scratch buffers for MeterFifo::drainInto

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GoniometerComponent)
};
