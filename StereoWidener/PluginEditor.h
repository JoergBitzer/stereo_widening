#pragma once

#include "PluginProcessor.h"
#include "tools/PresetHandler.h"
#include "tools/MidiModPitchState.h"


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

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    StereoWidenerAudioProcessor& m_processorRef;
    PresetComponent m_presetGUI;
#if WITH_MIDIKEYBOARD
    MidiKeyboardComponent m_keyboard;
    MidiModPitchBendStateComponent m_wheels;
#endif
    // plugin specific components
    StereoWidenerGUI m_editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoWidenerAudioProcessorEditor)
};
