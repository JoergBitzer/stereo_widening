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

protected:
    /** Called once per timer tick, right before repaint(). Default: nothing to do. */
    virtual void refresh() {}

private:
    void timerCallback() final { refresh(); repaint(); }
};
