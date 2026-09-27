/**
 * @file FrequencyAxis.h
 * @brief Frequency axis shared by the playgrounds' frequency displays: pixel <->
 *        frequency mapping and the grid with labels. Logarithmic (20 Hz-20 kHz, the
 *        default) or linear (e.g. 0-2 kHz, where comb teeth are evenly spaced).
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include "PlaygroundStyle.h"

struct FrequencyAxis
{
    static constexpr float kLogMinHz = 20.0f;
    static constexpr float kLogMaxHz = 20000.0f;

    juce::Rectangle<float> plot; // the area the axis spans
    bool logarithmic = true;
    float minHz = kLogMinHz;
    float maxHz = kLogMaxHz;

    static FrequencyAxis log(juce::Rectangle<float> plot) { return { plot, true, kLogMinHz, kLogMaxHz }; }
    static FrequencyAxis linear(juce::Rectangle<float> plot, float maxHz) { return { plot, false, 0.0f, maxHz }; }

    float xForFrequency(float hz) const
    {
        hz = juce::jlimit(minHz, maxHz, hz);
        const float proportion = logarithmic ? std::log(hz / minHz) / std::log(maxHz / minHz) : (hz - minHz) / (maxHz - minHz);
        return plot.getX() + plot.getWidth() * proportion;
    }

    float frequencyForX(float x) const
    {
        const float proportion = juce::jlimit(0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
        return logarithmic ? minHz * std::pow(maxHz / minHz, proportion) : minHz + (maxHz - minHz) * proportion;
    }

    /** Log: lines at 50, 100, 200, 500 Hz ... 10 kHz, "100", "1k", "10k" labelled.
     *  Linear: a line every 250 Hz, every 500 Hz labelled. */
    void drawGrid(juce::Graphics& g, const PlaygroundStyle& style, float fontSize) const
    {
        const auto drawLine = [&](float hz, const juce::String& label)
        {
            const float x = xForFrequency(hz);
            g.setColour(style.grid.withMultipliedAlpha(label.isNotEmpty() ? 1.0f : 0.5f));
            g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
            if (label.isNotEmpty())
            {
                g.setColour(style.text.withAlpha(0.6f));
                g.drawText(label, juce::Rectangle<float>(x + 0.2f * fontSize, plot.getBottom() - fontSize, 3.0f * fontSize, fontSize),
                           juce::Justification::centredLeft);
            }
        };
        const auto kiloLabel = [](float hz)
        {
            return hz >= 1000.0f ? juce::String(hz / 1000.0f, hz >= 10000.0f || std::fmod(hz, 1000.0f) < 1.0f ? 0 : 1) + "k"
                                 : juce::String(juce::roundToInt(hz));
        };

        if (logarithmic)
        {
            for (float hz : { 50.0f, 200.0f, 500.0f, 2000.0f, 5000.0f })
                drawLine(hz, {});
            for (float hz : { 100.0f, 1000.0f, 10000.0f })
                drawLine(hz, kiloLabel(hz));
            return;
        }
        for (float hz = 250.0f; hz < maxHz; hz += 250.0f)
            drawLine(hz, std::fmod(hz, 500.0f) < 1.0f ? kiloLabel(hz) : juce::String());
    }
};
