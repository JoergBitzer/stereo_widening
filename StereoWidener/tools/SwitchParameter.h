#pragma once

#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>

namespace jade
{
// Problem: juce::AudioParameterBool keeps the raw normalised value a host sends (setValue(0.23f) -> getValue() returns
// 0.23f), but saves and restores only a bool. The value the plugin reports then depends on how it got there, and
// pluginval 2.x fails the test "Discrete parameter state restoration" (strictness 8 and above). juce::AudioParameterChoice
// and juce::AudioParameterInt snap the value to a step; juce::AudioParameterBool is the only one that does not.
// (setValue and the value member of AudioParameterBool are private, so a subclass cannot repair it.)
//
// Solution: make an on/off parameter as a choice with the two entries Off and On. Controls work unchanged
// (juce::AudioProcessorValueTreeState::ButtonAttachment), and reading it is `getRawParameterValue(id)->load() >= 0.5f`.
inline std::unique_ptr<juce::AudioParameterChoice> makeSwitchParameter(const juce::String& id, const juce::String& name,
                                                                       bool defaultIsOn)
{
    const int kParameterVersion = 1;
    const int kIndexOff = 0;
    const int kIndexOn = 1;
    int defaultIndex = kIndexOff;
    if (defaultIsOn)
    {
        defaultIndex = kIndexOn;
    }
    return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(id, kParameterVersion), name,
                                                        juce::StringArray("Off", "On"), defaultIndex);
}
}
