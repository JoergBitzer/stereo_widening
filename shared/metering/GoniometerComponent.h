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
 * A point's distance from the centre is sqrt(S^2 + M^2); above 0 dBFS input this can
 * exceed 1 (the grid circle's own radius), so paint() clamps such points onto the
 * circle and draws them in red instead of letting them land anywhere in the panel.
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

    /** How long a point stays visible before it is dropped ("phosphor" persistence).
     *  Points arrive one per audio sample, so this is converted to a point count via
     *  state.getSampleRate() each refresh() tick -- independent of sample rate, unlike
     *  a fixed point count would be. */
    void setAfterglowTime(float seconds) noexcept { afterglowTime_s = juce::jmax(0.001f, seconds); }

private:
    void refresh() override; // drains the FIFO into history, then trims it to the afterglow time
    juce::Point<float> toScreen(float s, float m) const;

    StereoMeterState& state;
    juce::String label;
    juce::Rectangle<float> contentBounds; // set by paint() (post title/border), used by toScreen()

    std::deque<juce::Point<float>> history; // in normalised (-1..1, -1..1) S/M coordinates
    float afterglowTime_s = 0.2f;

    std::vector<float> drainX, drainY; // reused scratch buffers for MeterFifo::drainInto

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GoniometerComponent)
};
