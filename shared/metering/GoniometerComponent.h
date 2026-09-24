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

    /** Optional footer text (e.g. build info / version), drawn one line per entry in the
     *  panel's bottom-left corner -- the area that stays empty of grid/points, especially
     *  now that overload points are clamped onto the circle (see the file header). An
     *  empty array (the default) draws nothing, so this reusable component stays
     *  application-agnostic; the caller supplies whatever text (or none) fits its own
     *  plugin. */
    void setCornerText(juce::StringArray lines) { cornerTextLines = std::move(lines); }

    /** Colour for the primary series' non-overload points (default green, matching the
     *  established StereoAnalyzer look). */
    void setPrimaryColour(juce::Colour colour) noexcept { primaryColour = colour; }

    /** Adds a second signal, drawn in its own colour on top of the primary series but
     *  sharing the same grid/circle -- e.g. StereoWidener overlays input (primary,
     *  green) and output (secondary, blue) in one goniometer instead of needing two
     *  separate instances. Pass nullptr to remove it again. Both series still get the
     *  overload clamp-to-circle-and-turn-red treatment (see the file header). */
    void setSecondarySeries(StereoMeterState* stateToDisplay, juce::Colour colour);

private:
    void refresh() override; // drains both FIFOs into history, then trims to the afterglow time
    juce::Point<float> toScreen(float s, float m) const;
    static void drainSeries(StereoMeterState& s, std::deque<juce::Point<float>>& hist,
                             std::vector<float>& scratchX, std::vector<float>& scratchY);
    void drawSeries(juce::Graphics& g, const std::deque<juce::Point<float>>& hist,
                     juce::Colour colour, float pointDiameter) const;

    StereoMeterState& state;
    StereoMeterState* secondaryState = nullptr;
    juce::Colour primaryColour = MeterLookAndFeel::meterGood;
    juce::Colour secondaryColour = MeterLookAndFeel::meterGood;

    juce::String label;
    juce::Rectangle<float> contentBounds; // set by paint() (post title/border), used by toScreen()

    std::deque<juce::Point<float>> history;          // primary series, normalised (-1..1, -1..1) S/M
    std::deque<juce::Point<float>> secondaryHistory; // optional secondary series, same coordinates
    float afterglowTime_s = 0.2f;
    juce::StringArray cornerTextLines; // optional, see setCornerText()

    std::vector<float> drainX, drainY;   // reused scratch buffers for the primary series
    std::vector<float> drainX2, drainY2; // reused scratch buffers for the secondary series

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GoniometerComponent)
};
