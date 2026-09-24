#include <cmath>
#include "StereoWidener.h"

#include "PluginProcessor.h"

StereoWidenerAudio::StereoWidenerAudio(StereoWidenerAudioProcessor* processor)
:SynchronBlockProcessor(), m_processor(processor)
{
    // must stay in the same order as g_algorithmNames (StereoWidener.h)
    m_algorithms.push_back(std::make_unique<MSWidthBroadband>());
    m_algorithms.push_back(std::make_unique<MSWidthFiltered>());
}

void StereoWidenerAudio::prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels)
{
    m_sampleRate = sampleRate;
    // desiredSize <= 0 runs SynchronBlockProcessor in direct-through mode: processSynchronBlock
    // is called immediately with the host's own buffer, so the widener adds zero latency
    // beyond whatever the active algorithm itself reports (currently always 0).
    prepareSynchronProcessing(max_channels, 0);

    m_meterStateIn.prepare(sampleRate);
    m_meterStateOut.prepare(sampleRate);

    for (auto& algorithm : m_algorithms)
    {
        algorithm->prepare(sampleRate, max_samplesPerBlock);
        algorithm->reset();
    }

    m_crossfadeProgress.reset(sampleRate, (double) kCrossfadeSeconds);
    m_crossfadeProgress.setCurrentAndTargetValue(1.0f);
    m_crossfading = false;
    m_activeIndex = m_algorithmParam != nullptr ? m_algorithmParam->getIndex() : 0;
    m_targetIndex = m_activeIndex;
}

int StereoWidenerAudio::processSynchronBlock(juce::AudioBuffer<float> & buffer, juce::MidiBuffer &midiMessages, int NrOfBlocksSinceLastProcessBlock)
{
    juce::ignoreUnused(midiMessages, NrOfBlocksSinceLastProcessBlock);

    m_meterStateIn.processBlock(buffer);

    // Mono buffers have no stereo image to widen (bus layouts negotiated so input and
    // output channel counts always match, see isBusesLayoutSupported); pass through
    // unchanged rather than reading/writing a non-existent right channel.
    if (buffer.getNumChannels() < 2)
    {
        m_meterStateOut.processBlock(buffer);
        return 0;
    }

    const float width = m_widthParam != nullptr ? m_widthParam->get() * 0.01f : 1.0f; // 0-200 % -> 0-2
    const int selectedIndex = m_algorithmParam != nullptr ? m_algorithmParam->getIndex() : m_activeIndex;

    if (selectedIndex != m_activeIndex && !m_crossfading)
    {
        m_crossfading = true;
        m_targetIndex = selectedIndex;
        m_algorithms[(size_t) m_targetIndex]->reset(); // start the incoming algorithm with clean filter state
        m_crossfadeProgress.setCurrentAndTargetValue(0.0f);
        m_crossfadeProgress.setTargetValue(1.0f);
    }

    if (m_crossfading)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        m_crossfadeScratch.setSize(numChannels, numSamples, false, false, true);
        m_crossfadeScratch.makeCopyOf(buffer, true);

        m_algorithms[(size_t) m_activeIndex]->process(buffer, width);
        m_algorithms[(size_t) m_targetIndex]->process(m_crossfadeScratch, width);

        for (int i = 0; i < numSamples; ++i)
        {
            // equal-power crossfade: cos/sin instead of a linear ramp, so the summed
            // power stays constant through the fade instead of dipping in the middle
            const float t = m_crossfadeProgress.getNextValue();
            const float gainOutgoing = std::cos(t * juce::MathConstants<float>::halfPi);
            const float gainIncoming = std::sin(t * juce::MathConstants<float>::halfPi);
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* outgoing = buffer.getWritePointer(ch);
                auto* incoming = m_crossfadeScratch.getReadPointer(ch);
                outgoing[i] = outgoing[i] * gainOutgoing + incoming[i] * gainIncoming;
            }
        }

        if (!m_crossfadeProgress.isSmoothing())
        {
            m_activeIndex = m_targetIndex;
            m_crossfading = false;
        }
    }
    else
    {
        m_algorithms[(size_t) m_activeIndex]->process(buffer, width);
    }

    m_meterStateOut.processBlock(buffer);
    return 0;
}

void StereoWidenerAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>> &paramVector)
{
    const float interval = std::pow(10.0f, (float) -g_paramWidth.numDecimalPlaces);
    paramVector.push_back(std::make_unique<juce::AudioParameterFloat>(g_paramWidth.ID, g_paramWidth.name,
        juce::NormalisableRange<float>(g_paramWidth.minValue, g_paramWidth.maxValue, interval, g_paramWidth.skew),
        g_paramWidth.defaultValue,
        juce::AudioParameterFloatAttributes().withLabel(g_paramWidth.unitName)));

    paramVector.push_back(std::make_unique<juce::AudioParameterChoice>(g_paramAlgorithmID, g_paramAlgorithmName,
        g_algorithmNames, 0));
}

void StereoWidenerAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState> &vts)
{
    m_widthParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramWidth.ID));
    m_algorithmParam = dynamic_cast<juce::AudioParameterChoice*>(vts->getParameter(g_paramAlgorithmID));
}


StereoWidenerGUI::StereoWidenerGUI(StereoWidenerAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
:m_processor(p), m_apvts(apvts),
 m_levelMeterIn(p.m_algo.m_meterStateIn, "Input"),
 m_goniometer(p.m_algo.m_meterStateIn, "Goniometer"),
 m_levelMeterOut(p.m_algo.m_meterStateOut, "Output")
{
    // input drawn as the primary (green) series, output overlaid as the secondary
    // (blue) series, both sharing the one grid/circle -- see GoniometerComponent.h
    m_goniometer.setPrimaryColour(g_goniometerInColour);
    m_goniometer.setSecondarySeries(&p.m_algo.m_meterStateOut, g_goniometerOutColour);

    addAndMakeVisible(m_levelMeterIn);
    addAndMakeVisible(m_goniometer);
    addAndMakeVisible(m_levelMeterOut);

    m_widthLabel.setText("Width", juce::dontSendNotification);
    m_widthLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_widthLabel);

    m_widthKnob.setTextValueSuffix(" %");
    addAndMakeVisible(m_widthKnob);
    m_widthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        m_apvts, g_paramWidth.ID, m_widthKnob);

    for (int i = 0; i < g_algorithmNames.size(); ++i)
        m_algorithmBox.addItem(g_algorithmNames[i], i + 1); // JUCE ComboBox item IDs are 1-based
    addAndMakeVisible(m_algorithmBox);
    m_algorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        m_apvts, g_paramAlgorithmID, m_algorithmBox);
}

void StereoWidenerGUI::paint(juce::Graphics &g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).brighter(0.3f));
}

void StereoWidenerGUI::resized()
{
    const float scale = m_processor.getScaleFactor();
    m_levelMeterIn.setScaleFactor(scale);
    m_goniometer.setScaleFactor(scale);
    m_levelMeterOut.setScaleFactor(scale);

    auto r = getLocalBounds();
    const int rowGap = juce::roundToInt(g_rowGap * scale);

    // top row: input level meter | goniometer (in/out overlaid) | output level meter,
    // all sized to ~60% of their StereoAnalyzer equivalents (see PluginSettings.h)
    auto meterRow = r.removeFromTop(juce::roundToInt(g_meterRowHeight * scale));
    r.removeFromTop(rowGap);

    const int levelWidth = juce::roundToInt(g_levelMeterWidth * scale);
    m_levelMeterIn.setBounds(meterRow.removeFromLeft(levelWidth).reduced(juce::roundToInt(g_levelMeterPadding * scale)));
    m_levelMeterOut.setBounds(meterRow.removeFromRight(levelWidth).reduced(juce::roundToInt(g_levelMeterPadding * scale)));
    m_goniometer.setBounds(meterRow.reduced(juce::roundToInt(g_goniometerPadding * scale)));

    // middle row: the big Width knob, centred
    auto knobRow = r.removeFromTop(juce::roundToInt(g_widthKnobRowHeight * scale));
    r.removeFromTop(rowGap);

    const int knobSize = juce::roundToInt(g_widthKnobSize * scale);
    const int labelHeight = juce::roundToInt(g_widthKnobLabelHeight * scale);
    const int textBoxHeight = labelHeight;
    m_widthKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobSize, textBoxHeight);

    auto knobArea = knobRow.withSizeKeepingCentre(knobSize, labelHeight + knobSize + textBoxHeight);
    m_widthLabel.setBounds(knobArea.removeFromTop(labelHeight));
    m_widthKnob.setBounds(knobArea);

    // bottom row: the algorithm selector, centred
    auto algoRow = r.removeFromTop(juce::roundToInt(g_algorithmRowHeight * scale));
    const int boxWidth = juce::jmin(juce::roundToInt(g_algorithmBoxWidth * scale), algoRow.getWidth());
    m_algorithmBox.setBounds(algoRow.withSizeKeepingCentre(boxWidth, algoRow.getHeight()));
}
