#pragma once

#include <memory>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "tools/SynchronBlockProcessor.h"
#include "PluginSettings.h"
#include "algorithms/StereoAlgorithm.h"
#include "algorithms/MSWidthBroadband.h"
#include "algorithms/MSWidthFiltered.h"
#include "../shared/metering/StereoMeterState.h"
#include "../shared/metering/GoniometerComponent.h"
#include "../shared/metering/LevelMeterComponent.h"

class StereoWidenerAudioProcessor;

// Width: 0 % collapses the side signal to mono, 100 % is unity (unchanged from input),
// 200 % doubles the side signal. See algorithms/MSWidthBroadband.h.
const struct
{
	const std::string ID = "width";
	const std::string name = "Width";
	const std::string unitName = "%";
	const float minValue = 0.0f;
	const float maxValue = 200.0f;
	const float defaultValue = 100.0f;
	const float skew = 1.0f;
	const int numDecimalPlaces = 0;
}g_paramWidth;

constexpr const char* g_paramAlgorithmID = "algorithm";
constexpr const char* g_paramAlgorithmName = "Algorithm";

// Must stay in the same order as the algorithm instances StereoWidenerAudio's
// constructor creates in algorithms/ -- see the comment there. Two algorithms so far,
// specifically so switching between them (a crossfade, see processSynchronBlock) has
// something audibly different to exercise: 2.1 M/S width, broadband vs. bass-mono.
const juce::StringArray g_algorithmNames { "M/S Width (Broadband)", "M/S Width (Filtered / Bass Mono)" };

class StereoWidenerAudio : public SynchronBlockProcessor
{
public:
    StereoWidenerAudio(StereoWidenerAudioProcessor* processor);
    void prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels);
    virtual int processSynchronBlock(juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock);

    // parameter handling
  	void addParameter(std::vector < std::unique_ptr<juce::RangedAudioParameter>>& paramVector);
    void prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>&  vts);

    // Neither algorithm currently adds latency (see StereoAlgorithm::getLatencySamples());
    // a future linear-phase mode would need to report the max over all algorithms here
    // instead, and StereoWidenerAudioProcessor would need to call setLatencySamples()
    // again on every algorithm switch (plan2.md Phase 4, "Latency").
    int getLatency(){return 0;};

    StereoMeterState m_meterStateIn;
    StereoMeterState m_meterStateOut;

private:
	StereoWidenerAudioProcessor* m_processor;
    double m_sampleRate = 44100.0;

    juce::AudioParameterFloat* m_widthParam = nullptr;
    juce::AudioParameterChoice* m_algorithmParam = nullptr;

    std::vector<std::unique_ptr<StereoAlgorithm>> m_algorithms;
    int m_activeIndex = 0;

    // Equal-power crossfade between the outgoing and incoming algorithm on a switch
    // (plan2.md Phase 3: "Algorithm switching with an equal-power crossfade
    // (20-50 ms)"), so changing modes never clicks. Both algorithms process the whole
    // block every tick while a crossfade is in progress (only while m_crossfading is
    // true -- negligible extra CPU the rest of the time, since only the active
    // algorithm runs then).
    juce::SmoothedValue<float> m_crossfadeProgress;
    bool m_crossfading = false;
    int m_targetIndex = 0;
    juce::AudioBuffer<float> m_crossfadeScratch;
    static constexpr float kCrossfadeSeconds = 0.03f;
};

class StereoWidenerGUI : public juce::Component
{
public:
	StereoWidenerGUI(StereoWidenerAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

	void paint(juce::Graphics& g) override;
	void resized() override;
private:
	StereoWidenerAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;

    LevelMeterComponent m_levelMeterIn;
    GoniometerComponent m_goniometer;
    LevelMeterComponent m_levelMeterOut;

    juce::Label m_widthLabel;
    juce::Slider m_widthKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_widthAttachment;

    juce::ComboBox m_algorithmBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> m_algorithmAttachment;
};
