#include "LevelMeterComponent.h"

LevelMeterComponent::LevelMeterComponent(StereoMeterState& stateToDisplay)
    : state(stateToDisplay)
{
    startTimerHz(30);
}

LevelMeterComponent::~LevelMeterComponent()
{
    stopTimer();
}

void LevelMeterComponent::timerCallback()
{
    repaint();
}

float LevelMeterComponent::dbToFraction(float db) const noexcept
{
    return juce::jlimit(0.0f, 1.0f, (db - rangeMinDb) / (rangeMaxDb - rangeMinDb));
}

void LevelMeterComponent::drawBar(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& name,
                                   float rmsDb, float peakDb) const
{
    g.setColour(juce::Colours::black);
    g.fillRect(bounds);

    auto barArea = bounds.reduced(bounds.getWidth() * 0.15f, 2.0f);

    const float rmsFraction = dbToFraction(rmsDb);
    auto rmsRect = barArea.removeFromBottom(barArea.getHeight() * rmsFraction);
    g.setColour(juce::Colours::limegreen);
    g.fillRect(rmsRect);

    // peak: a thin horizontal line at the peak level
    const float peakFraction = dbToFraction(peakDb);
    const float peakY = bounds.getBottom() - peakFraction * (bounds.getHeight() - 4.0f);
    g.setColour(juce::Colours::white);
    g.drawHorizontalLine((int) peakY, bounds.getX() + 2.0f, bounds.getRight() - 2.0f);

    g.setColour(juce::Colours::lightgrey);
    g.setFont(11.0f);
    g.drawText(name, bounds.removeFromTop(14.0f), juce::Justification::centred);
}

void LevelMeterComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    auto bounds = getLocalBounds().toFloat();
    auto barsArea = bounds.withTrimmedBottom(16.0f);
    const float barWidth = barsArea.getWidth() / 4.0f;

    drawBar(g, barsArea.removeFromLeft(barWidth), "L",
            state.getRmsDb(StereoMeterState::Left), state.getPeakDb(StereoMeterState::Left));
    drawBar(g, barsArea.removeFromLeft(barWidth), "R",
            state.getRmsDb(StereoMeterState::Right), state.getPeakDb(StereoMeterState::Right));
    drawBar(g, barsArea.removeFromLeft(barWidth), "M",
            state.getRmsDb(StereoMeterState::Mid), state.getPeakDb(StereoMeterState::Mid));
    drawBar(g, barsArea.removeFromLeft(barWidth), "S",
            state.getRmsDb(StereoMeterState::Side), state.getPeakDb(StereoMeterState::Side));

    // width estimate readout (S - M in dB): 0 dB = equal power, very negative = narrow/mono
    auto textArea = bounds.removeFromBottom(16.0f);
    g.setColour(juce::Colours::lightgrey);
    g.setFont(11.0f);
    juce::String widthText = "S-M " + juce::String(state.getWidthEstimateDb(), 1) + " dB";
    g.drawText(widthText, textArea, juce::Justification::centred);
}

void LevelMeterComponent::resized()
{
}
