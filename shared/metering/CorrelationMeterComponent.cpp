#include "CorrelationMeterComponent.h"

CorrelationMeterComponent::CorrelationMeterComponent(StereoMeterState& stateToDisplay)
    : state(stateToDisplay)
{
    startTimerHz(30);
}

CorrelationMeterComponent::~CorrelationMeterComponent()
{
    stopTimer();
}

void CorrelationMeterComponent::timerCallback()
{
    // the meter engine already integrates over rmsTimeConstant_s; this extra bit of
    // smoothing (GUI-side, ~100 ms at 30 Hz) only avoids a jittery display at 30 fps
    const float target = state.getCorrelation();
    displayedValue += 0.3f * (target - displayedValue);
    repaint();
}

void CorrelationMeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    g.setColour(juce::Colours::black);
    g.fillRect(bounds);

    // scale ticks at -1, -0.5, 0, +0.5, +1
    g.setColour(juce::Colours::darkgrey);
    for (float v = -1.0f; v <= 1.0001f; v += 0.5f)
    {
        const float x = bounds.getX() + (v + 1.0f) * 0.5f * bounds.getWidth();
        g.drawVerticalLine((int) x, bounds.getY(), bounds.getBottom());
    }

    // bar from the centre (0) to the current value
    const float value = juce::jlimit(-1.0f, 1.0f, displayedValue);
    const float centreX = bounds.getX() + 0.5f * bounds.getWidth();
    const float valueX = bounds.getX() + (value + 1.0f) * 0.5f * bounds.getWidth();
    const auto barColour = value < 0.0f ? juce::Colours::red
                                         : juce::Colours::limegreen.withAlpha(0.5f + 0.5f * value);
    g.setColour(barColour);
    g.fillRect(juce::jmin(centreX, valueX), bounds.getY(), std::abs(valueX - centreX), bounds.getHeight());

    g.setColour(juce::Colours::white);
    g.setFont(11.0f);
    g.drawText(juce::String(value, 2), bounds, juce::Justification::centred);
}

void CorrelationMeterComponent::resized()
{
}
