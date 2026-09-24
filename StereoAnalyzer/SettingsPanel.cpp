#include "SettingsPanel.h"
#include "StereoAnalyzer.h"

namespace
{
    constexpr int kRowHeight = 44;
    constexpr int kLabelHeight = 16;
    constexpr int kPadding = 10;
}

SettingsPanel::SettingsPanel(juce::AudioProcessorValueTreeState& apvts)
{
    setUpRow(m_integrationRow, apvts, g_paramIntegration.ID, g_paramIntegration.name, g_paramIntegration.unitName);
    setUpRow(m_peakHoldRow, apvts, g_paramPeakHold.ID, g_paramPeakHold.name, g_paramPeakHold.unitName);
    setUpRow(m_peakDecayRow, apvts, g_paramPeakDecay.ID, g_paramPeakDecay.name, g_paramPeakDecay.unitName);
    setUpRow(m_afterglowRow, apvts, g_paramAfterglow.ID, g_paramAfterglow.name, g_paramAfterglow.unitName);

    setSize(width, height);
}

void SettingsPanel::setUpRow(Row& row, juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                              const juce::String& labelText, const juce::String& unit)
{
    row.label.setText(labelText, juce::dontSendNotification);
    addAndMakeVisible(row.label);

    // the slider's own text box does not pick up the parameter's unit label
    // automatically, so it is added here explicitly. The decimal precision, though,
    // does come from the parameter (its interval, see makeFloatParameter in
    // StereoAnalyzer.cpp) rather than from the slider, so it stays in sync with
    // whatever a host's own generic parameter view would show.
    row.slider.setTextValueSuffix(" " + unit);
    addAndMakeVisible(row.slider);

    row.attachment = std::make_unique<SliderAttachment>(apvts, paramID, row.slider);
}

void SettingsPanel::resized()
{
    auto r = getLocalBounds().reduced(kPadding);
    for (Row* row : { &m_integrationRow, &m_peakHoldRow, &m_peakDecayRow, &m_afterglowRow })
    {
        auto rowArea = r.removeFromTop(kRowHeight);
        row->label.setBounds(rowArea.removeFromTop(kLabelHeight));
        row->slider.setBounds(rowArea);
    }
}
