#include "CorrelationMeterComponent.h"

namespace
{
    // GUI-side smoothing of the displayed value, in addition to StereoMeterState's own
    // integration time: purely cosmetic, avoids a jittery bar at the 30 Hz repaint rate.
    // 0.3 means the bar covers ~70% of a step change within one timer tick (~33 ms).
    constexpr float kDisplaySmoothingFactor = 0.3f;

    constexpr float kTickStep = 0.5f; // scale ticks at -1, -0.5, 0, +0.5, +1
    constexpr float kBoundsInset = 2.0f;
}

CorrelationMeterComponent::CorrelationMeterComponent(StereoMeterState& stateToDisplay)
    : state(stateToDisplay)
{
}

void CorrelationMeterComponent::refresh()
{
    const float target = state.getCorrelation();
    displayedValue += kDisplaySmoothingFactor * (target - displayedValue);
}

void CorrelationMeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(kBoundsInset);
    g.setColour(MeterLookAndFeel::background);
    g.fillRect(bounds);

    g.setColour(MeterLookAndFeel::grid);
    for (float v = -1.0f; v <= 1.0001f; v += kTickStep)
    {
        const float x = bounds.getX() + (v + 1.0f) * 0.5f * bounds.getWidth();
        g.drawVerticalLine((int) x, bounds.getY(), bounds.getBottom());
    }

    // bar from the centre (0) to the current value
    const float value = juce::jlimit(-1.0f, 1.0f, displayedValue);
    const float centreX = bounds.getX() + 0.5f * bounds.getWidth();
    const float valueX = bounds.getX() + (value + 1.0f) * 0.5f * bounds.getWidth();
    const auto barColour = value < 0.0f ? MeterLookAndFeel::outOfPhase
                                         : MeterLookAndFeel::meterFill.withAlpha(0.5f + 0.5f * value);
    g.setColour(barColour);
    g.fillRect(juce::jmin(centreX, valueX), bounds.getY(), std::abs(valueX - centreX), bounds.getHeight());

    g.setColour(MeterLookAndFeel::text);
    g.setFont(MeterLookAndFeel::labelFontSize);
    g.drawText(juce::String(value, 2), bounds, juce::Justification::centred);
}

void CorrelationMeterComponent::resized()
{
}
