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
