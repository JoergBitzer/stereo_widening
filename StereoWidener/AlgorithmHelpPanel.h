/**
 * @file AlgorithmHelpPanel.h
 * @brief Content of the algorithm help popup (shown in a juce::CallOutBox from the "?"
 *        button next to the algorithm selector, matching the pattern already used for
 *        StereoAnalyzer's Settings popup, see StereoAnalyzer/SettingsPanel.h): the
 *        active algorithm's name, its description, and a citation to a written source
 *        -- this plugin is a teaching example as much as a tool (planing.md section 5).
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class AlgorithmHelpPanel : public juce::Component
{
public:
    /** title is the algorithm's getName(); description is its getDescription()
     *  (already including the citation). Both are plain text captured at construction
     *  time, so the popup always reflects whichever algorithm was selected when the
     *  "?" button was clicked, even if the selection changes while the popup is open. */
    AlgorithmHelpPanel(const juce::String& title, const juce::String& description);

    void paint(juce::Graphics& g) override;

    static constexpr int width = 340;

private:
    static constexpr int kPadding = 12;
    static constexpr float kTitleFontSize = 15.0f;
    static constexpr float kBodyFontSize = 13.0f;
    static constexpr int kTitleHeight = 22;
    static constexpr int kTitleBodyGap = 6;

    juce::String m_title;
    juce::TextLayout m_bodyLayout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlgorithmHelpPanel)
};
