/**
 * @file MeterLookAndFeel.h
 * @brief Visual constants shared by the metering components (GoniometerComponent,
 *        CorrelationMeterComponent, LevelMeterComponent), so they read as one
 *        consistent instrument panel instead of three independently-styled widgets.
 *
 * Constants specific to a single component (e.g. the goniometer's circle margin) stay
 * local to that component's .cpp file; only what is actually shared lives here.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_graphics/juce_graphics.h>

namespace MeterLookAndFeel
{
    // -- colours -----------------------------------------------------------------
    const juce::Colour background = juce::Colours::black;
    const juce::Colour grid       = juce::Colours::darkgrey;  // scale lines, axes
    const juce::Colour text       = juce::Colours::lightgrey; // labels, readouts
    const juce::Colour meterFill  = juce::Colours::limegreen; // RMS bars, in-phase correlation
    const juce::Colour peakLine   = juce::Colours::white;     // peak indicator
    const juce::Colour outOfPhase = juce::Colours::red;       // correlation < 0

    // -- shared sizes --------------------------------------------------------------
    constexpr float labelFontSize = 11.0f; // small readouts (dB values, axis labels, "L"/"R"/"M")
    constexpr float titleFontSize = 13.0f; // component title (e.g. "Goniometer")
    constexpr int   refreshRateHz = 30;    // how often MeterComponentBase repaints
}
