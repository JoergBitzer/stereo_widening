/**
 * @file PlaygroundStyle.h
 * @brief Shared look for the playgrounds' live graphics, so every algorithm's display
 *        reads as part of one plugin (plan_changeGUI.md, design goal 3). Everything is
 *        taken from the current look-and-feel, so both themes work automatically.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

struct PlaygroundStyle
{
    juce::Colour background; // display panel: the knobs' own fill, like the meter displays
    juce::Colour text;
    juce::Colour grid;       // axes, reference lines
    juce::Colour accent;     // the thing the controls change (curve, wedge, handles)

    static PlaygroundStyle of(const juce::Component& component)
    {
        auto& lf = component.getLookAndFeel();
        const auto background = lf.findColour(juce::Slider::backgroundColourId);
        auto text = lf.findColour(juce::Label::textColourId);
        // The Day theme's knob grey is mid-light, so its normal (grey) text would only
        // reach about 3:1 contrast on it -- darkened, about 6:1.
        if (background.getPerceivedBrightness() > 0.5f)
            text = text.darker(1.0f);
        return { background, text, text.withAlpha(0.25f), lf.findColour(juce::Slider::thumbColourId) };
    }

    static constexpr float kCornerSize = 6.0f;  // display panel corner radius, unscaled
    static constexpr float kFontSize = 12.0f;   // display text, unscaled
};
