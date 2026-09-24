/**
 * @file SettingsPanel.h
 * @brief Content of the Settings popup (shown in a juce::CallOutBox from the main GUI's
 *        "Settings..." button): the settings that don't need to be on the main view --
 *        RMS/correlation integration time, peak hold time, peak decay rate, and the
 *        goniometer's afterglow time. See StereoAnalyzer.h for the parameter
 *        definitions (g_paramIntegration, g_paramPeakHold, g_paramPeakDecay,
 *        g_paramAfterglow) and StereoMeterState.h/GoniometerComponent.h for what they do.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class SettingsPanel : public juce::Component
{
public:
    explicit SettingsPanel(juce::AudioProcessorValueTreeState& apvts);

    void resized() override;

    static constexpr int width = 260;
    static constexpr int height = 196; // 4 rows * kRowHeight(44) + 2 * kPadding(10), see .cpp

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Row
    {
        juce::Label label;
        juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        std::unique_ptr<SliderAttachment> attachment;
    };

    void setUpRow(Row& row, juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                  const juce::String& labelText, const juce::String& unit);

    Row m_integrationRow, m_peakHoldRow, m_peakDecayRow, m_afterglowRow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
