/**
 * @file MeterLookAndFeel.h
 * @brief Visual constants and the shared panel chrome (background, border, title) for
 *        the metering components (GoniometerComponent, CorrelationMeterComponent,
 *        LevelMeterComponent), so they read as one consistent instrument panel instead
 *        of three independently-styled widgets.
 *
 * Constants specific to a single component (e.g. the goniometer's circle margin) stay
 * local to that component's .cpp file; only what is actually shared lives here.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace MeterLookAndFeel
{
    // -- colours -----------------------------------------------------------------
    const juce::Colour background = juce::Colours::black;
    const juce::Colour grid       = juce::Colours::darkgrey;  // scale lines, axes
    const juce::Colour text       = juce::Colours::lightgrey; // labels, readouts
    const juce::Colour panelBorder = juce::Colours::grey;     // panel outline

    // Three-zone "traffic light" language, shared by the level meter and the
    // correlation meter, so both speak the same visual vocabulary:
    //   good    - safe / mono-compatible / plenty of headroom
    //   caution - worth watching
    //   danger  - a real problem (near clipping, or significant phase cancellation)
    const juce::Colour meterGood    = juce::Colours::limegreen;
    const juce::Colour meterCaution = juce::Colours::orange;
    const juce::Colour meterDanger  = juce::Colours::red;
    const juce::Colour peakLine     = juce::Colours::white;

    // -- shared sizes --------------------------------------------------------------
    constexpr float labelFontSize      = 11.0f; // small readouts (dB values, axis labels, "L"/"R"/"M")
    constexpr float smallLabelFontSize = 9.0f;  // tight spaces: per-bar numeric readouts, endpoint labels
    constexpr float titleFontSize      = 13.0f; // panel title (e.g. "Goniometer", "Levels")
    constexpr float titleBoxHeight     = 18.0f;
    constexpr float borderThickness    = 1.0f;
    constexpr int   refreshRateHz      = 30;    // how often MeterComponentBase repaints

    /** Fills the background, draws the panel border, and (if title is non-empty) the
     *  title text in a reserved top strip. Returns the remaining content area below
     *  the title for the component to draw its actual meter into. */
    inline juce::Rectangle<float> drawPanel(juce::Graphics& g, juce::Rectangle<float> bounds,
                                             const juce::String& title)
    {
        g.setColour(background);
        g.fillRect(bounds);
        g.setColour(panelBorder);
        g.drawRect(bounds, borderThickness);

        auto content = bounds.reduced(borderThickness);
        if (title.isNotEmpty())
        {
            auto titleArea = content.removeFromTop(titleBoxHeight);
            g.setColour(text);
            g.setFont(titleFontSize);
            g.drawText(title, titleArea, juce::Justification::centred);
        }
        return content;
    }
}
