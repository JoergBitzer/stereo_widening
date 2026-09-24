#pragma once

#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

#include "tools/SynchronBlockProcessor.h"
#include "PluginSettings.h"
#include "../shared/metering/StereoMeterState.h"
#include "../shared/metering/GoniometerComponent.h"
#include "../shared/metering/CorrelationMeterComponent.h"
#include "../shared/metering/LevelMeterComponent.h"

class StereoAnalyzerAudioProcessor;

// integration time for the RMS and correlation meters (see StereoMeterState)
const struct
{
	const std::string ID = "integration";
	const std::string name = "Integration";
	const juce::StringArray choices { "Fast (100 ms)", "Medium (300 ms)", "Slow (1000 ms)" };
	const float timeConstants_s[3] = { 0.1f, 0.3f, 1.0f };
	const int defaultIndex = 1;
}g_paramIntegration;


class StereoAnalyzerAudio : public SynchronBlockProcessor
{
public:
    StereoAnalyzerAudio(StereoAnalyzerAudioProcessor* processor);
    void prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels);
    virtual int processSynchronBlock(juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock);

    // parameter handling
  	void addParameter(std::vector < std::unique_ptr<juce::RangedAudioParameter>>& paramVector);
    void prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>&  vts);

    // some necessary info for the host: the analyzer is a pure observer, so it never
    // adds latency (SynchronBlockProcessor runs in direct-through mode, see .cpp)
    int getLatency(){return 0;};

    StereoMeterState m_meterState;

private:
	StereoAnalyzerAudioProcessor* m_processor;
    double m_sampleRate = 44100.0;
    juce::AudioParameterChoice* m_integrationParam = nullptr;
    int m_lastIntegrationIndex = -1;
};

class StereoAnalyzerGUI : public juce::Component
{
public:
	StereoAnalyzerGUI(StereoAnalyzerAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

	void paint(juce::Graphics& g) override;
	void resized() override;
private:
	StereoAnalyzerAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;

    GoniometerComponent m_goniometer;
    CorrelationMeterComponent m_correlationMeter;
    LevelMeterComponent m_levelMeter;
    juce::ComboBox m_integrationBox;
    juce::Label m_integrationLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> m_integrationAttachment;
};
