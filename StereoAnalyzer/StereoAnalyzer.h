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

// The three meter ballistics settings, all continuous AudioParameterFloats (so they are
// automatable and saved with the plugin state, like every other parameter here) shown
// together on the Settings popup (see SettingsPanel.h) rather than cluttering the main
// view. See StereoMeterState.h for what each one actually does.
const struct
{
	const std::string ID = "integration";
	const std::string name = "Integration";
	const std::string unitName = "s";
	const float minValue = 0.05f;
	const float maxValue = 2.0f;
	const float defaultValue = 0.3f;
	const float skew = 0.4f; // biases the slider towards the shorter, more commonly used times
	const int numDecimalPlaces = 2;
}g_paramIntegration;

const struct
{
	const std::string ID = "peakHold";
	const std::string name = "Peak Hold";
	const std::string unitName = "s";
	const float minValue = 0.0f;
	const float maxValue = 5.0f;
	const float defaultValue = 1.5f;
	const float skew = 1.0f;
	const int numDecimalPlaces = 2;
}g_paramPeakHold;

const struct
{
	const std::string ID = "peakDecay";
	const std::string name = "Peak Decay";
	const std::string unitName = "dB/s";
	const float minValue = 3.0f;
	const float maxValue = 60.0f;
	const float defaultValue = 20.0f;
	const float skew = 1.0f;
	const int numDecimalPlaces = 1;
}g_paramPeakDecay;

// How long a goniometer point stays visible before it fades out ("phosphor"
// persistence). GUI-only: read by StereoAnalyzerGUI, not by StereoAnalyzerAudio, since
// it never affects the meter engine, only how many of GoniometerComponent's already-
// pushed points are drawn.
const struct
{
	const std::string ID = "afterglow";
	const std::string name = "Afterglow";
	const std::string unitName = "s";
	const float minValue = 0.05f;
	const float maxValue = 2.0f;
	const float defaultValue = 0.2f;
	const float skew = 0.5f; // biases the slider towards the shorter, more commonly used times
	const int numDecimalPlaces = 2;
}g_paramAfterglow;


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
    juce::AudioParameterFloat* m_integrationParam = nullptr;
    juce::AudioParameterFloat* m_peakHoldParam = nullptr;
    juce::AudioParameterFloat* m_peakDecayParam = nullptr;
    // last value applied to m_meterState, to avoid recomputing every block; -1 forces
    // the first processSynchronBlock() call to always apply the (possibly loaded) state
    float m_lastIntegration = -1.0f;
    float m_lastPeakHold = -1.0f;
    float m_lastPeakDecay = -1.0f;
};

class StereoAnalyzerGUI : public juce::Component, private juce::Timer
{
public:
	StereoAnalyzerGUI(StereoAnalyzerAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

	void paint(juce::Graphics& g) override;
	void resized() override;
private:
    void showSettings();
    // polls the Afterglow parameter and pushes it to m_goniometer on change, so a
    // change from the Settings popup or from host automation is picked up either way
    // (the same poll-and-apply-on-change pattern StereoAnalyzerAudio uses for its own
    // three parameters, just on the GUI thread and only for this GUI-only setting)
    void timerCallback() override;

	StereoAnalyzerAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;

    GoniometerComponent m_goniometer;
    CorrelationMeterComponent m_correlationMeter;
    LevelMeterComponent m_levelMeter;
    juce::TextButton m_settingsButton { "Settings..." };

    juce::AudioParameterFloat* m_afterglowParam = nullptr;
    float m_lastAfterglow = -1.0f;
};
