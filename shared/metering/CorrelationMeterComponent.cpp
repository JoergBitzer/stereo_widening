#include "CorrelationMeterComponent.h"

namespace
{
    // GUI-side smoothing of the displayed value, in addition to StereoMeterState's own
    // integration time: purely cosmetic, avoids a jittery bar at the 30 Hz repaint rate.
    // 0.3 means the bar covers ~70% of a step change within one timer tick (~33 ms).
    constexpr float kDisplaySmoothingFactor = 0.3f;

    constexpr float kTickStep = 0.5f; // scale ticks at -1, -0.5, 0, +0.5, +1
    constexpr float kBarInsetX = 4.0f;
    constexpr float kBarInsetY = 1.0f;
    constexpr float kEndpointLabelRowHeight = 12.0f;

    // Zone boundaries for the bar colour: same green/amber/red language as
    // LevelMeterComponent. A rule of thumb, not a formal standard: "safe" above +0.3,
    // "worth watching" between -0.3 and +0.3 (decorrelated/wide, or drifting negative),
    // "a real phase problem" below -0.3.
    constexpr float kDangerThreshold = -0.3f;
    constexpr float kCautionThreshold = 0.3f;

    juce::Colour zoneColour(float value)
    {
        if (value < kDangerThreshold)
            return MeterLookAndFeel::meterDanger;
        if (value < kCautionThreshold)
            return MeterLookAndFeel::meterCaution;
        return MeterLookAndFeel::meterGood;
    }
}

CorrelationMeterComponent::CorrelationMeterComponent(StereoMeterState& stateToDisplay, juce::String labelText)
    : state(stateToDisplay), label(std::move(labelText))
{
}

void CorrelationMeterComponent::refresh()
{
    const float target = state.getCorrelation();
    displayedValue += kDisplaySmoothingFactor * (target - displayedValue);
}

void CorrelationMeterComponent::paint(juce::Graphics& g)
{
    auto content = MeterLookAndFeel::drawPanel(g, getLocalBounds().toFloat(), label);
    auto endpointLabelsRow = content.removeFromBottom(kEndpointLabelRowHeight);
    auto bounds = content.reduced(kBarInsetX, kBarInsetY);

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
    g.setColour(zoneColour(value));
    g.fillRect(juce::jmin(centreX, valueX), bounds.getY(), std::abs(valueX - centreX), bounds.getHeight());

    g.setColour(MeterLookAndFeel::text);
    g.setFont(MeterLookAndFeel::labelFontSize);
    g.drawText(juce::String(value, 2), bounds, juce::Justification::centred);

    const float thirdWidth = endpointLabelsRow.getWidth() / 3.0f;
    g.setFont(MeterLookAndFeel::smallLabelFontSize);
    g.drawText("-1 OUT OF PHASE", endpointLabelsRow.removeFromLeft(thirdWidth), juce::Justification::centredLeft);
    g.drawText("0 WIDE", endpointLabelsRow.removeFromLeft(thirdWidth), juce::Justification::centred);
    g.drawText("+1 MONO", endpointLabelsRow, juce::Justification::centredRight);
}

void CorrelationMeterComponent::resized()
{
}
