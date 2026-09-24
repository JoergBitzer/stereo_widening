/**
 * @file MeterComponentBase.h
 * @brief Common base for the metering GUI components: owns the repaint timer so the
 *        three components (goniometer, correlation meter, level meter) don't each
 *        repeat the same start/stop-timer boilerplate at a hand-copied refresh rate.
 *
 * A subclass overrides refresh() to pull new data from its StereoMeterState before the
 * repaint (e.g. GoniometerComponent drains its FIFO, CorrelationMeterComponent smooths
 * its displayed value); a component that only reads atomics directly in paint()
 * (LevelMeterComponent) can leave the default no-op refresh().
 *
 * Also owns scaleFactor: the plugin editor resizes by dragging its corner, and every
 * pixel-unit constant in these components' paint() (font sizes, line thicknesses, box
 * sizes, gaps -- anything not already a fraction of the component's own bounds) needs
 * to scale with it, or text and fixed-size details stay the same absolute size while
 * the rest of the layout grows or shrinks around them. The plugin editor computes this
 * the same way the template's preset handler and MIDI keyboard already do (current
 * width / g_minGuiSize_x, see PluginEditor.cpp) and calls setScaleFactor() on each
 * meter component from StereoAnalyzerGUI::resized().
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

#include "MeterLookAndFeel.h"

class MeterComponentBase : public juce::Component, protected juce::Timer
{
public:
    MeterComponentBase() { startTimerHz(MeterLookAndFeel::refreshRateHz); }
    ~MeterComponentBase() override { stopTimer(); }

    void setScaleFactor(float newScale) noexcept
    {
        scaleFactor = newScale;
        repaint();
    }

protected:
    /** Called once per timer tick, right before repaint(). Default: nothing to do. */
    virtual void refresh() {}

    float scaleFactor = 1.0f;

private:
    void timerCallback() final { refresh(); repaint(); }
};
