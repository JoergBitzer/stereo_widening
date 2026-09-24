#include "LevelMeterComponent.h"

namespace
{
    constexpr float kBarSideMarginFraction = 0.15f; // horizontal gap between adjacent bars, as a fraction of bar width
    constexpr float kBarVerticalInset = 2.0f;
    constexpr float kNameLabelHeight = 14.0f; // "L"/"R"/"M"/"S" label above each bar
    constexpr float kWidthReadoutHeight = 16.0f; // "S-M x.x dB" row below the bars
    constexpr int kNumBars = 4; // L, R, M, S
}

LevelMeterComponent::LevelMeterComponent(StereoMeterState& stateToDisplay)
    : state(stateToDisplay)
{
}

float LevelMeterComponent::dbToFraction(float db) const noexcept
{
    return juce::jlimit(0.0f, 1.0f, (db - rangeMinDb) / (rangeMaxDb - rangeMinDb));
}

void LevelMeterComponent::drawBar(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& name,
                                   float rmsDb, float peakDb) const
{
    g.setColour(MeterLookAndFeel::background);
    g.fillRect(bounds);

    auto barArea = bounds.reduced(bounds.getWidth() * kBarSideMarginFraction, kBarVerticalInset);

    const float rmsFraction = dbToFraction(rmsDb);
    auto rmsRect = barArea.removeFromBottom(barArea.getHeight() * rmsFraction);
    g.setColour(MeterLookAndFeel::meterFill);
    g.fillRect(rmsRect);

    // peak: a thin horizontal line at the peak level
    const float peakFraction = dbToFraction(peakDb);
    const float peakY = bounds.getBottom() - peakFraction * (bounds.getHeight() - 2.0f * kBarVerticalInset);
    g.setColour(MeterLookAndFeel::peakLine);
    g.drawHorizontalLine((int) peakY, bounds.getX() + kBarVerticalInset, bounds.getRight() - kBarVerticalInset);

    g.setColour(MeterLookAndFeel::text);
    g.setFont(MeterLookAndFeel::labelFontSize);
    g.drawText(name, bounds.removeFromTop(kNameLabelHeight), juce::Justification::centred);
}

void LevelMeterComponent::paint(juce::Graphics& g)
{
    g.fillAll(MeterLookAndFeel::background);

    auto bounds = getLocalBounds().toFloat();
    auto barsArea = bounds.withTrimmedBottom(kWidthReadoutHeight);
    const float barWidth = barsArea.getWidth() / (float) kNumBars;

    drawBar(g, barsArea.removeFromLeft(barWidth), "L",
            state.getRmsDb(StereoMeterState::Left), state.getPeakDb(StereoMeterState::Left));
    drawBar(g, barsArea.removeFromLeft(barWidth), "R",
            state.getRmsDb(StereoMeterState::Right), state.getPeakDb(StereoMeterState::Right));
    drawBar(g, barsArea.removeFromLeft(barWidth), "M",
            state.getRmsDb(StereoMeterState::Mid), state.getPeakDb(StereoMeterState::Mid));
    drawBar(g, barsArea.removeFromLeft(barWidth), "S",
            state.getRmsDb(StereoMeterState::Side), state.getPeakDb(StereoMeterState::Side));

    // width estimate readout (S - M in dB): 0 dB = equal power, very negative = narrow/mono
    auto textArea = bounds.removeFromBottom(kWidthReadoutHeight);
    g.setColour(MeterLookAndFeel::text);
    g.setFont(MeterLookAndFeel::labelFontSize);
    juce::String widthText = "S-M " + juce::String(state.getWidthEstimateDb(), 1) + " dB";
    g.drawText(widthText, textArea, juce::Justification::centred);
}

void LevelMeterComponent::resized()
{
}
