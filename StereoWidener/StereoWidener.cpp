#include <cmath>
#include "StereoWidener.h"

#include "PluginProcessor.h"

StereoWidenerAudio::StereoWidenerAudio(StereoWidenerAudioProcessor* processor)
:SynchronBlockProcessor(), m_processor(processor)
{
    // must stay in the same order as g_algorithmNames (StereoWidener.h)
    m_algorithms.push_back(std::make_unique<MSWidthBroadband>());
    m_algorithms.push_back(std::make_unique<MSWidthFiltered>());

    // user-configurable default (plan2.md Phase 4, "Global ini file"), previously a
    // fixed compiled-in constant -- see GlobalSettings.h
    if (auto* filtered = dynamic_cast<MSWidthFiltered*>(m_algorithms[1].get()))
        filtered->setHighShelfGainDb(m_globalSettings.getHighShelfGainDb());
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

    StereoAlgorithmParams params;
    params.width = m_widthParam != nullptr ? m_widthParam->get() * 0.01f : 1.0f; // 0-200 % -> 0-2
    params.auxLeft = m_bassCutoffParam != nullptr ? m_bassCutoffParam->get() : g_paramBassCutoff.defaultValue;
    params.auxRight = m_highShelfFreqParam != nullptr ? m_highShelfFreqParam->get() : g_paramHighShelfFreq.defaultValue;
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

        m_algorithms[(size_t) m_activeIndex]->process(buffer, params);
        m_algorithms[(size_t) m_targetIndex]->process(m_crossfadeScratch, params);

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
        m_algorithms[(size_t) m_activeIndex]->process(buffer, params);
    }

    m_meterStateOut.processBlock(buffer);
    return 0;
}

namespace
{
    // Builds a float parameter the same way as every g_param* struct in this file: the
    // slider step (interval) is set to 10^-numDecimalPlaces, e.g. 1 for
    // numDecimalPlaces = 0, so both the knob and any host's generic parameter view show
    // a clean "150 Hz" instead of "150.000000 Hz" (see StereoAnalyzer.cpp, same pattern).
    template <typename ParamDef>
    std::unique_ptr<juce::AudioParameterFloat> makeFloatParameter(const ParamDef& p)
    {
        const float interval = std::pow(10.0f, (float) -p.numDecimalPlaces);
        return std::make_unique<juce::AudioParameterFloat>(p.ID, p.name,
            juce::NormalisableRange<float>(p.minValue, p.maxValue, interval, p.skew),
            p.defaultValue,
            juce::AudioParameterFloatAttributes().withLabel(p.unitName));
    }

    // Like makeFloatParameter, but the parameter's own displayed text (used by this
    // plugin's knobs via Slider::textFromValueFunction below, and independently by any
    // host's generic parameter/automation view) reads "Off" once the value crosses
    // offThreshold, instead of a plain number -- see MSWidthFiltered::
    // kBassCutoffOffThreshold / kHighShelfOffThreshold. offIsBelow selects which side of
    // the threshold counts as "off": true for Bass Cutoff (off below 40 Hz), false for
    // High Shelf (off above 16 kHz).
    template <typename ParamDef>
    std::unique_ptr<juce::AudioParameterFloat> makeFrequencyParameterWithOff(const ParamDef& p, float offThreshold, bool offIsBelow)
    {
        const float interval = std::pow(10.0f, (float) -p.numDecimalPlaces);
        auto attributes = juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([offThreshold, offIsBelow](float value, int) -> juce::String
            {
                const bool isOff = offIsBelow ? (value < offThreshold) : (value > offThreshold);
                return isOff ? juce::String("Off") : juce::String(juce::roundToInt(value)) + " Hz";
            })
            .withValueFromStringFunction([offThreshold, offIsBelow](const juce::String& text) -> float
            {
                if (text.trim().equalsIgnoreCase("off"))
                    return offIsBelow ? offThreshold - 1.0f : offThreshold + 1.0f;
                return text.getFloatValue();
            });
        return std::make_unique<juce::AudioParameterFloat>(p.ID, p.name,
            juce::NormalisableRange<float>(p.minValue, p.maxValue, interval, p.skew),
            p.defaultValue, attributes);
    }

    // Like makeFrequencyParameterWithOff, but with a *true* logarithmic mapping (equal
    // Hz ratios get equal knob rotation) instead of a linear-or-power-law one: needed
    // for a range spanning several octaves (High Shelf: 1000-16500 Hz, over four),
    // where linear would cram the musically useful low end into a sliver of the knob
    // (see the comment on g_paramHighShelfFreq, StereoWidener.h). Always "off above
    // offThreshold" (High Shelf's only need so far); unlike the linear version, the log
    // compression at the top of the range keeps that Off zone a small rotation sliver
    // on its own, without needing to also keep it narrow in absolute Hz.
    template <typename ParamDef>
    std::unique_ptr<juce::AudioParameterFloat> makeLogFrequencyParameterWithOff(const ParamDef& p, float offThreshold)
    {
        juce::NormalisableRange<float> range(p.minValue, p.maxValue,
            [](float rangeStart, float rangeEnd, float normalised) // convertFrom0To1
            {
                return rangeStart * std::pow(rangeEnd / rangeStart, normalised);
            },
            [](float rangeStart, float rangeEnd, float value) // convertTo0To1
            {
                return std::log(value / rangeStart) / std::log(rangeEnd / rangeStart);
            },
            [](float, float, float value) // snapToLegalValue: whole Hz steps
            {
                return std::round(value);
            });

        auto attributes = juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([offThreshold](float value, int) -> juce::String
            {
                return value > offThreshold ? juce::String("Off") : juce::String(juce::roundToInt(value)) + " Hz";
            })
            .withValueFromStringFunction([offThreshold](const juce::String& text) -> float
            {
                return text.trim().equalsIgnoreCase("off") ? offThreshold + 1.0f : text.getFloatValue();
            });

        return std::make_unique<juce::AudioParameterFloat>(p.ID, p.name, range, p.defaultValue, attributes);
    }
}

void StereoWidenerAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>> &paramVector)
{
    paramVector.push_back(makeFloatParameter(g_paramWidth));
    paramVector.push_back(makeFrequencyParameterWithOff(g_paramBassCutoff, MSWidthFiltered::kBassCutoffOffThreshold, true));
    paramVector.push_back(makeLogFrequencyParameterWithOff(g_paramHighShelfFreq, MSWidthFiltered::kHighShelfOffThreshold));

    paramVector.push_back(std::make_unique<juce::AudioParameterChoice>(g_paramAlgorithmID, g_paramAlgorithmName,
        g_algorithmNames, 0));
}

void StereoWidenerAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState> &vts)
{
    m_widthParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramWidth.ID));
    m_bassCutoffParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramBassCutoff.ID));
    m_highShelfFreqParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramHighShelfFreq.ID));
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

    // same build/version footer as StereoAnalyzer's goniometer (see
    // docs/algorithms/phase2_stereo_analyzer.md), but as a single line here rather than
    // stacked, since this goniometer's bottom-left corner is much smaller
    const juce::String versionText = "v" + juce::String(PLUGIN_VERSION_MAJOR) + "."
                                    + juce::String(PLUGIN_VERSION_MINOR) + "." + juce::String(PLUGIN_VERSION_PATCH);
    m_goniometer.setCornerText({ "Built at Jade Hochschule Oldenburg - " + versionText });

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

    // custom text display (not just a unit suffix) so the "Off" zone at the bottom of
    // this knob's range (see g_paramBassCutoff, MSWidthFiltered::kBassCutoffOffThreshold)
    // reads as "Off" instead of e.g. "39 Hz"
    m_auxLeftLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_auxLeftLabel);
    m_auxLeftKnob.textFromValueFunction = [](double value) -> juce::String
    {
        return value < MSWidthFiltered::kBassCutoffOffThreshold ? "Off"
             : juce::String(juce::roundToInt(value)) + " Hz";
    };
    m_auxLeftKnob.valueFromTextFunction = [](const juce::String& text) -> double
    {
        return text.trim().equalsIgnoreCase("off") ? (double) g_paramBassCutoff.minValue : text.getDoubleValue();
    };
    addAndMakeVisible(m_auxLeftKnob);
    m_auxLeftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        m_apvts, g_paramBassCutoff.ID, m_auxLeftKnob);

    // mirrors the left knob: "Off" above MSWidthFiltered::kHighShelfOffThreshold
    m_auxRightLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_auxRightLabel);
    m_auxRightKnob.textFromValueFunction = [](double value) -> juce::String
    {
        return value > MSWidthFiltered::kHighShelfOffThreshold ? "Off"
             : juce::String(juce::roundToInt(value)) + " Hz";
    };
    m_auxRightKnob.valueFromTextFunction = [](const juce::String& text) -> double
    {
        return text.trim().equalsIgnoreCase("off") ? (double) g_paramHighShelfFreq.maxValue : text.getDoubleValue();
    };
    addAndMakeVisible(m_auxRightKnob);
    m_auxRightAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        m_apvts, g_paramHighShelfFreq.ID, m_auxRightKnob);

    m_helpButton.onClick = [this] { showAlgorithmHelp(); };
    addAndMakeVisible(m_helpButton);

    for (int i = 0; i < g_algorithmNames.size(); ++i)
        m_algorithmBox.addItem(g_algorithmNames[i], i + 1); // JUCE ComboBox item IDs are 1-based
    addAndMakeVisible(m_algorithmBox);
    m_algorithmBox.onChange = [this] { updateAuxKnobsForActiveAlgorithm(); };
    m_algorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        m_apvts, g_paramAlgorithmID, m_algorithmBox);

    updateAuxKnobsForActiveAlgorithm(); // onChange above only fires on a later *change*, not this initial state
}

void StereoWidenerGUI::showAlgorithmHelp()
{
    const int index = juce::jlimit(0, m_processor.m_algo.getNumAlgorithms() - 1, m_algorithmBox.getSelectedItemIndex());
    const auto& algorithm = m_processor.m_algo.getAlgorithm(index);
    auto panel = std::make_unique<AlgorithmHelpPanel>(algorithm.getName(), algorithm.getDescription());
    juce::CallOutBox::launchAsynchronously(std::move(panel), m_helpButton.getScreenBounds(), nullptr);
}

void StereoWidenerGUI::updateAuxKnobsForActiveAlgorithm()
{
    const int index = juce::jlimit(0, m_processor.m_algo.getNumAlgorithms() - 1, m_algorithmBox.getSelectedItemIndex());
    const auto& algorithm = m_processor.m_algo.getAlgorithm(index);

    const auto leftInfo = algorithm.getAuxLeftInfo();
    m_auxLeftKnob.setEnabled(leftInfo.enabled);
    m_auxLeftLabel.setText(leftInfo.enabled ? leftInfo.label : juce::String(), juce::dontSendNotification);

    const auto rightInfo = algorithm.getAuxRightInfo();
    m_auxRightKnob.setEnabled(rightInfo.enabled);
    m_auxRightLabel.setText(rightInfo.enabled ? rightInfo.label : juce::String(), juce::dontSendNotification);
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

    // middle row: the big Width knob, flanked by the two smaller aux knobs (their
    // meaning/enabled-state depends on the active algorithm, see
    // updateAuxKnobsForActiveAlgorithm())
    auto knobRow = r.removeFromTop(juce::roundToInt(g_widthKnobRowHeight * scale));
    r.removeFromTop(rowGap);

    const int knobSize = juce::roundToInt(g_widthKnobSize * scale);
    const int labelHeight = juce::roundToInt(g_widthKnobLabelHeight * scale);
    const int textBoxHeight = labelHeight;
    const int auxKnobSize = juce::roundToInt(g_auxKnobSize * scale);
    const int auxLabelHeight = juce::roundToInt(g_auxKnobLabelHeight * scale);
    const int auxGap = juce::roundToInt(g_auxKnobGap * scale);

    const int totalKnobsWidth = auxKnobSize + auxGap + knobSize + auxGap + auxKnobSize;
    auto knobsCentred = knobRow.withSizeKeepingCentre(totalKnobsWidth, knobRow.getHeight());

    auto auxLeftArea = knobsCentred.removeFromLeft(auxKnobSize);
    knobsCentred.removeFromLeft(auxGap);
    auto widthArea = knobsCentred.removeFromLeft(knobSize);
    knobsCentred.removeFromLeft(auxGap);
    auto auxRightArea = knobsCentred; // remaining width is exactly auxKnobSize

    m_widthKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobSize, textBoxHeight);
    auto widthStack = widthArea.withSizeKeepingCentre(knobSize, labelHeight + knobSize + textBoxHeight);
    m_widthLabel.setBounds(widthStack.removeFromTop(labelHeight));
    m_widthKnob.setBounds(widthStack);

    m_auxLeftKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, auxKnobSize, auxLabelHeight);
    auto auxLeftStack = auxLeftArea.withSizeKeepingCentre(auxKnobSize, auxLabelHeight + auxKnobSize + auxLabelHeight);
    m_auxLeftLabel.setBounds(auxLeftStack.removeFromTop(auxLabelHeight));
    m_auxLeftKnob.setBounds(auxLeftStack);

    m_auxRightKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, auxKnobSize, auxLabelHeight);
    auto auxRightStack = auxRightArea.withSizeKeepingCentre(auxKnobSize, auxLabelHeight + auxKnobSize + auxLabelHeight);
    m_auxRightLabel.setBounds(auxRightStack.removeFromTop(auxLabelHeight));
    m_auxRightKnob.setBounds(auxRightStack);

    // bottom row: the "?" help button, then the algorithm selector, centred as a group
    auto algoRow = r.removeFromTop(juce::roundToInt(g_algorithmRowHeight * scale));
    const int helpSize = juce::roundToInt(g_helpButtonSize * scale);
    const int helpGap = juce::roundToInt(g_helpButtonGap * scale);
    const int boxWidth = juce::jmin(juce::roundToInt(g_algorithmBoxWidth * scale),
                                     algoRow.getWidth() - helpSize - helpGap);
    auto algoGroup = algoRow.withSizeKeepingCentre(helpSize + helpGap + boxWidth, algoRow.getHeight());
    m_helpButton.setBounds(algoGroup.removeFromLeft(helpSize).withSizeKeepingCentre(helpSize, helpSize));
    algoGroup.removeFromLeft(helpGap);
    m_algorithmBox.setBounds(algoGroup);
}
