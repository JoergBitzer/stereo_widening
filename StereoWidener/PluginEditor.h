#pragma once

#include "PluginProcessor.h"
#include "tools/PresetHandler.h"
#include "tools/MidiModPitchState.h"
#include "PluginLookAndFeel.h"

#include "StereoWidener.h"

//==============================================================================
class StereoWidenerAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:

    explicit StereoWidenerAudioProcessorEditor (StereoWidenerAudioProcessor&);
    ~StereoWidenerAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Resizes the window to fit whichever algorithm is now active (StereoWidenerGUI::
    // getRequiredContentHeight()) -- wired to m_editor.onActiveAlgorithmChanged in the
    // constructor, and also called once manually right after, since the GUI's own
    // initial call (from its constructor) happens before that wiring exists yet. Most
    // algorithms need the same, compact size; Multiband Width (Phase 5 algorithm 2.7)
    // needs a taller window for its 6-parameter grid, so the window actually changes
    // shape on selecting/leaving it -- see docs/algorithms (phase5 multiband write-up).
    void updateWindowSizeForActiveAlgorithm();

    // Flips m_lookAndFeel's theme, persists the choice (GlobalSettings::
    // saveUseDayTheme(), immediately -- unlike the continuously-changing GUI scale
    // factor, a theme toggle is a rare discrete event, so there is no reason to defer
    // writing it to the destructor), updates the button's own icon, and repaints.
    void toggleTheme();

    // Declared FIRST (constructed first, destroyed LAST): Component's own destructor
    // may still consult getLookAndFeel() while child components below are being torn
    // down, so this must outlive every component that references it. setLookAndFeel()
    // is also explicitly cleared at the top of ~StereoWidenerAudioProcessorEditor()'s
    // body, before any member destruction begins, as a second layer of safety.
    StereoWidenerLookAndFeel m_lookAndFeel;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    StereoWidenerAudioProcessor& m_processorRef;
    PresetComponent m_presetGUI;
    juce::TextButton m_themeButton; // day/night toggle, see toggleTheme()
#if WITH_MIDIKEYBOARD
    MidiKeyboardComponent m_keyboard;
    MidiModPitchBendStateComponent m_wheels;
#endif
    // plugin specific components
    StereoWidenerGUI m_editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoWidenerAudioProcessorEditor)
};
