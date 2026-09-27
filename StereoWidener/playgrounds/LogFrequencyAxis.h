/**
 * @file LogFrequencyAxis.h
 * @brief Log frequency axis (20 Hz-20 kHz) shared by the playgrounds' frequency
 *        displays: pixel <-> frequency mapping and the grid with decade labels.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include "PlaygroundStyle.h"

struct LogFrequencyAxis
{
    static constexpr float kMinHz = 20.0f;
    static constexpr float kMaxHz = 20000.0f;

    juce::Rectangle<float> plot; // the area the axis spans

    float xForFrequency(float hz) const
    {
        return plot.getX() + plot.getWidth() * std::log(hz / kMinHz) / std::log(kMaxHz / kMinHz);
    }

    float frequencyForX(float x) const
    {
        const float proportion = juce::jlimit(0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
        return kMinHz * std::pow(kMaxHz / kMinHz, proportion);
    }

    /** Vertical grid lines at 50, 100, 200, 500 Hz ... 10 kHz (decades stronger), with
     *  "100", "1k", "10k" along the bottom edge. */
    void drawGrid(juce::Graphics& g, const PlaygroundStyle& style, float fontSize) const
    {
        struct GridLine { float hz; const char* label; };
        for (auto line : { GridLine { 50.0f, nullptr }, GridLine { 100.0f, "100" }, GridLine { 200.0f, nullptr },
                           GridLine { 500.0f, nullptr }, GridLine { 1000.0f, "1k" }, GridLine { 2000.0f, nullptr },
                           GridLine { 5000.0f, nullptr }, GridLine { 10000.0f, "10k" } })
        {
            const float x = xForFrequency(line.hz);
            g.setColour(style.grid.withMultipliedAlpha(line.label != nullptr ? 1.0f : 0.5f));
            g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
            if (line.label != nullptr)
            {
                g.setColour(style.text.withAlpha(0.6f));
                g.drawText(line.label, juce::Rectangle<float>(x + 0.2f * fontSize, plot.getBottom() - fontSize, 3.0f * fontSize, fontSize),
                           juce::Justification::centredLeft);
            }
        }
    }
};
