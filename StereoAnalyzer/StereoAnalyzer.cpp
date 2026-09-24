#include <math.h>
#include "StereoAnalyzer.h"

#include "PluginProcessor.h"

StereoAnalyzerAudio::StereoAnalyzerAudio(StereoAnalyzerAudioProcessor* processor)
:SynchronBlockProcessor(), m_processor(processor)
{
}

void StereoAnalyzerAudio::prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels)
{
    juce::ignoreUnused(max_samplesPerBlock,max_channels);
    m_sampleRate = sampleRate;
    // desiredSize <= 0 runs SynchronBlockProcessor in direct-through mode: processSynchronBlock
    // is called immediately with the host's own buffer, so the analyzer adds zero latency
    // (see tools/SynchronBlockProcessor.h). A fixed block size is not needed here: the
    // metering math in StereoMeterState is a per-sample recursive filter, not block-based.
    prepareSynchronProcessing(max_channels, 0);

    const int index = m_integrationParam != nullptr ? m_integrationParam->getIndex() : g_paramIntegration.defaultIndex;
    m_meterState.prepare(sampleRate, g_paramIntegration.timeConstants_s[index]);
    m_lastIntegrationIndex = index;
}

int StereoAnalyzerAudio::processSynchronBlock(juce::AudioBuffer<float> & buffer, juce::MidiBuffer &midiMessages, int NrOfBlocksSinceLastProcessBlock)
{
    juce::ignoreUnused(midiMessages, NrOfBlocksSinceLastProcessBlock);

    // apply a changed Integration setting. Re-preparing resets the running RMS/correlation
    // state, which is the expected behaviour when the user changes the meter ballistics
    // (comparable to switching ballistics on a hardware meter).
    if (m_integrationParam != nullptr)
    {
        const int index = m_integrationParam->getIndex();
        if (index != m_lastIntegrationIndex)
        {
            m_meterState.prepare(m_sampleRate, g_paramIntegration.timeConstants_s[index]);
            m_lastIntegrationIndex = index;
        }
    }

    m_meterState.processBlock(buffer);
    // the analyzer is a pure observer: the buffer passes through unchanged
    return 0;
}

void StereoAnalyzerAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>> &paramVector)
{
    paramVector.push_back(std::make_unique<AudioParameterChoice>(g_paramIntegration.ID,
        g_paramIntegration.name,
        g_paramIntegration.choices,
        g_paramIntegration.defaultIndex));
}

void StereoAnalyzerAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState> &vts)
{
    m_integrationParam = dynamic_cast<AudioParameterChoice*>(vts->getParameter(g_paramIntegration.ID));
}


StereoAnalyzerGUI::StereoAnalyzerGUI(StereoAnalyzerAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
:m_processor(p), m_apvts(apvts),
 m_goniometer(p.m_algo.m_meterState, "Goniometer"),
 m_correlationMeter(p.m_algo.m_meterState, "Correlation"),
 m_levelMeter(p.m_algo.m_meterState, "Levels")
{
    addAndMakeVisible(m_goniometer);
    addAndMakeVisible(m_correlationMeter);
    addAndMakeVisible(m_levelMeter);

    m_integrationLabel.setText(g_paramIntegration.name, juce::dontSendNotification);
    m_integrationLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_integrationLabel);

    m_integrationBox.addItemList(g_paramIntegration.choices, 1);
    addAndMakeVisible(m_integrationBox);
    m_integrationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        m_apvts, g_paramIntegration.ID, m_integrationBox);
}

void StereoAnalyzerGUI::paint(juce::Graphics &g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).brighter(0.3f));
}

void StereoAnalyzerGUI::resized()
{
	auto r = getLocalBounds();

    auto controlsRow = r.removeFromBottom(g_controlsRowHeight);
    m_integrationBox.setBounds(controlsRow.removeFromRight(g_integrationBoxWidth).reduced(g_controlPadding));
    m_integrationLabel.setBounds(controlsRow.removeFromRight(g_integrationLabelWidth).reduced(g_controlPadding));

    auto correlationRow = r.removeFromBottom(g_correlationRowHeight);
    m_correlationMeter.setBounds(correlationRow.reduced(g_correlationPaddingX, g_correlationPaddingY));

    const int levelWidth = juce::jmin(g_levelMeterMaxWidth, r.getWidth() / g_levelMeterWidthDivisor);
    m_levelMeter.setBounds(r.removeFromRight(levelWidth).reduced(g_levelMeterPadding));
    m_goniometer.setBounds(r.reduced(g_goniometerPadding));
}
