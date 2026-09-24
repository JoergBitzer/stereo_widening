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

    // meter ballistics: StereoWidener has no per-project override for these yet (unlike
    // StereoAnalyzer's Settings popup), so the global settings file's defaults are the
    // only source for now -- see GlobalSettings.h
    m_meterStateIn.prepare(sampleRate, m_globalSettings.getMeterIntegrationTimeS(),
                            m_globalSettings.getMeterPeakDecayDbPerS(), m_globalSettings.getMeterPeakHoldTimeS());
    m_meterStateOut.prepare(sampleRate, m_globalSettings.getMeterIntegrationTimeS(),
                             m_globalSettings.getMeterPeakDecayDbPerS(), m_globalSettings.getMeterPeakHoldTimeS());

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

    // Utilities (Phase 4 step 2, planing.md 2.13 + 2.2): applied once, after whichever
    // width algorithm just ran, regardless of which one is active -- see
    // UtilityProcessor.h.
    UtilityParams utilityParams;
    utilityParams.rotationDeg = m_rotationParam != nullptr ? m_rotationParam->get() : 0.0f;
    utilityParams.balance = m_balanceParam != nullptr ? m_balanceParam->get() * 0.01f : 0.0f; // % -> -1..1
    utilityParams.invertL = m_invertLParam != nullptr && m_invertLParam->get();
    utilityParams.invertR = m_invertRParam != nullptr && m_invertRParam->get();
    utilityParams.swapLR = m_swapLRParam != nullptr && m_swapLRParam->get();
    utilityParams.monitorMode = m_monitorModeParam != nullptr ? m_monitorModeParam->getIndex() : 0;
    m_utilityProcessor.process(buffer, utilityParams);

    m_meterStateOut.processBlock(buffer);
    return 0;
}

namespace
{
    // Builds a float parameter the same way as every g_param* struct in this file: the
    // slider step (interval) is set to 10^-numDecimalPlaces, e.g. 1 for
    // numDecimalPlaces = 0, so both the knob and any host's generic parameter view show
    // a clean "150 Hz" instead of "150.000000 Hz" (see StereoAnalyzer.cpp, same pattern).
    // defaultValue is passed explicitly (rather than always using p.defaultValue) so the
    // caller can seed it from GlobalSettings' "last used" state (Phase 4 step 2) --
    // clamped defensively, in case a hand-edited settings file has a stale/out-of-range
    // value from before a range changed.
    template <typename ParamDef>
    std::unique_ptr<juce::AudioParameterFloat> makeFloatParameter(const ParamDef& p, float defaultValue)
    {
        defaultValue = juce::jlimit(p.minValue, p.maxValue, defaultValue);
        const float interval = std::pow(10.0f, (float) -p.numDecimalPlaces);
        return std::make_unique<juce::AudioParameterFloat>(p.ID, p.name,
            juce::NormalisableRange<float>(p.minValue, p.maxValue, interval, p.skew),
            defaultValue,
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
    std::unique_ptr<juce::AudioParameterFloat> makeFrequencyParameterWithOff(const ParamDef& p, float offThreshold,
                                                                              bool offIsBelow, float defaultValue)
    {
        defaultValue = juce::jlimit(p.minValue, p.maxValue, defaultValue);
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
            defaultValue, attributes);
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
    std::unique_ptr<juce::AudioParameterFloat> makeLogFrequencyParameterWithOff(const ParamDef& p, float offThreshold,
                                                                                 float defaultValue)
    {
        defaultValue = juce::jlimit(p.minValue, p.maxValue, defaultValue);
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

        return std::make_unique<juce::AudioParameterFloat>(p.ID, p.name, range, defaultValue, attributes);
    }
}

void StereoWidenerAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>> &paramVector)
{
    // "Last used state" (plan2.md Phase 4 step 1): a brand new instance starts from
    // whatever was last saved to the global settings file (StereoWidenerAudioProcessor's
    // destructor), or the compiled-in default if there is none yet (e.g. the very first
    // run). A DAW project's own saved state, restored afterwards via
    // setStateInformation(), always overrides this -- it unconditionally replaces the
    // whole parameter tree, which runs strictly after this constructor-time code.
    const auto lastUsed = [this](const std::string& id, double fallback)
    {
        return m_globalSettings.getLastUsedParam(id, fallback);
    };

    paramVector.push_back(makeFloatParameter(g_paramWidth, (float) lastUsed(g_paramWidth.ID, g_paramWidth.defaultValue)));
    paramVector.push_back(makeFrequencyParameterWithOff(g_paramBassCutoff, MSWidthFiltered::kBassCutoffOffThreshold, true,
        (float) lastUsed(g_paramBassCutoff.ID, g_paramBassCutoff.defaultValue)));
    paramVector.push_back(makeLogFrequencyParameterWithOff(g_paramHighShelfFreq, MSWidthFiltered::kHighShelfOffThreshold,
        (float) lastUsed(g_paramHighShelfFreq.ID, g_paramHighShelfFreq.defaultValue)));

    const int lastAlgorithmIndex = juce::jlimit(0, g_algorithmNames.size() - 1,
        (int) lastUsed(g_paramAlgorithmID, 0.0));
    paramVector.push_back(std::make_unique<juce::AudioParameterChoice>(g_paramAlgorithmID, g_paramAlgorithmName,
        g_algorithmNames, lastAlgorithmIndex));

    // Utilities (Phase 4 step 2)
    paramVector.push_back(makeFloatParameter(g_paramRotation, (float) lastUsed(g_paramRotation.ID, g_paramRotation.defaultValue)));
    paramVector.push_back(makeFloatParameter(g_paramBalance, (float) lastUsed(g_paramBalance.ID, g_paramBalance.defaultValue)));
    paramVector.push_back(std::make_unique<juce::AudioParameterBool>(g_paramInvertL.ID, g_paramInvertL.name,
        lastUsed(g_paramInvertL.ID, 0.0) > 0.5));
    paramVector.push_back(std::make_unique<juce::AudioParameterBool>(g_paramInvertR.ID, g_paramInvertR.name,
        lastUsed(g_paramInvertR.ID, 0.0) > 0.5));
    paramVector.push_back(std::make_unique<juce::AudioParameterBool>(g_paramSwapLR.ID, g_paramSwapLR.name,
        lastUsed(g_paramSwapLR.ID, 0.0) > 0.5));
    const int lastMonitorMode = juce::jlimit(0, g_monitorModeNames.size() - 1,
        (int) lastUsed(g_paramMonitorModeID, 0.0));
    paramVector.push_back(std::make_unique<juce::AudioParameterChoice>(g_paramMonitorModeID, g_paramMonitorModeName,
        g_monitorModeNames, lastMonitorMode));
}

void StereoWidenerAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState> &vts)
{
    m_widthParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramWidth.ID));
    m_bassCutoffParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramBassCutoff.ID));
    m_highShelfFreqParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramHighShelfFreq.ID));
    m_algorithmParam = dynamic_cast<juce::AudioParameterChoice*>(vts->getParameter(g_paramAlgorithmID));

    m_rotationParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramRotation.ID));
    m_balanceParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramBalance.ID));
    m_invertLParam = dynamic_cast<juce::AudioParameterBool*>(vts->getParameter(g_paramInvertL.ID));
    m_invertRParam = dynamic_cast<juce::AudioParameterBool*>(vts->getParameter(g_paramInvertR.ID));
    m_swapLRParam = dynamic_cast<juce::AudioParameterBool*>(vts->getParameter(g_paramSwapLR.ID));
    m_monitorModeParam = dynamic_cast<juce::AudioParameterChoice*>(vts->getParameter(g_paramMonitorModeID));
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

    // Utilities (Phase 4 step 2), applied regardless of the selected algorithm -- see
    // UtilityProcessor.h
    m_utilitiesTitle.setText("Utilities", juce::dontSendNotification);
    m_utilitiesTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_utilitiesTitle);

    m_rotationLabel.setText("Rotation", juce::dontSendNotification);
    m_rotationLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_rotationLabel);
    m_rotationKnob.setTextValueSuffix(juce::String::fromUTF8(" \xc2\xb0")); // degree sign
    addAndMakeVisible(m_rotationKnob);
    m_rotationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        m_apvts, g_paramRotation.ID, m_rotationKnob);

    m_balanceLabel.setText("Balance", juce::dontSendNotification);
    m_balanceLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_balanceLabel);
    m_balanceKnob.setTextValueSuffix(" %");
    addAndMakeVisible(m_balanceKnob);
    m_balanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        m_apvts, g_paramBalance.ID, m_balanceKnob);

    m_swapLRButton.setClickingTogglesState(true);
    addAndMakeVisible(m_swapLRButton);
    m_swapLRAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        m_apvts, g_paramSwapLR.ID, m_swapLRButton);

    m_invertLButton.setClickingTogglesState(true);
    addAndMakeVisible(m_invertLButton);
    m_invertLAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        m_apvts, g_paramInvertL.ID, m_invertLButton);

    m_invertRButton.setClickingTogglesState(true);
    addAndMakeVisible(m_invertRButton);
    m_invertRAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        m_apvts, g_paramInvertR.ID, m_invertRButton);

    for (int i = 0; i < g_monitorModeNames.size(); ++i)
        m_monitorModeBox.addItem(g_monitorModeNames[i], i + 1);
    addAndMakeVisible(m_monitorModeBox);
    m_monitorModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        m_apvts, g_paramMonitorModeID, m_monitorModeBox);
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

    // algorithm row: the "?" help button, then the algorithm selector, centred as a group
    auto algoRow = r.removeFromTop(juce::roundToInt(g_algorithmRowHeight * scale));
    r.removeFromTop(rowGap);
    const int helpSize = juce::roundToInt(g_helpButtonSize * scale);
    const int helpGap = juce::roundToInt(g_helpButtonGap * scale);
    const int boxWidth = juce::jmin(juce::roundToInt(g_algorithmBoxWidth * scale),
                                     algoRow.getWidth() - helpSize - helpGap);
    auto algoGroup = algoRow.withSizeKeepingCentre(helpSize + helpGap + boxWidth, algoRow.getHeight());
    m_helpButton.setBounds(algoGroup.removeFromLeft(helpSize).withSizeKeepingCentre(helpSize, helpSize));
    algoGroup.removeFromLeft(helpGap);
    m_algorithmBox.setBounds(algoGroup);

    // Utilities section (Phase 4 step 2): a title, Rotation/Balance knobs, then a row
    // of toggle buttons and the Monitor selector -- applied regardless of the selected
    // algorithm, see UtilityProcessor.h.
    m_utilitiesTitle.setBounds(r.removeFromTop(juce::roundToInt(g_utilitiesTitleHeight * scale)));
    r.removeFromTop(rowGap);

    auto utilKnobRow = r.removeFromTop(juce::roundToInt(g_utilitiesKnobSize * scale)
                                        + 2 * juce::roundToInt(g_utilitiesKnobLabelHeight * scale));
    r.removeFromTop(rowGap);

    const int utilKnobSize = juce::roundToInt(g_utilitiesKnobSize * scale);
    const int utilLabelHeight = juce::roundToInt(g_utilitiesKnobLabelHeight * scale);
    const int utilKnobGap = juce::roundToInt(g_utilitiesKnobGap * scale);
    const int totalUtilKnobsWidth = utilKnobSize + utilKnobGap + utilKnobSize;
    auto utilKnobsCentred = utilKnobRow.withSizeKeepingCentre(totalUtilKnobsWidth, utilKnobRow.getHeight());
    auto rotationArea = utilKnobsCentred.removeFromLeft(utilKnobSize);
    utilKnobsCentred.removeFromLeft(utilKnobGap);
    auto balanceArea = utilKnobsCentred;

    m_rotationKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, utilKnobSize, utilLabelHeight);
    auto rotationStack = rotationArea.withSizeKeepingCentre(utilKnobSize, utilLabelHeight + utilKnobSize + utilLabelHeight);
    m_rotationLabel.setBounds(rotationStack.removeFromTop(utilLabelHeight));
    m_rotationKnob.setBounds(rotationStack);

    m_balanceKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, utilKnobSize, utilLabelHeight);
    auto balanceStack = balanceArea.withSizeKeepingCentre(utilKnobSize, utilLabelHeight + utilKnobSize + utilLabelHeight);
    m_balanceLabel.setBounds(balanceStack.removeFromTop(utilLabelHeight));
    m_balanceKnob.setBounds(balanceStack);

    auto toggleRow = r.removeFromTop(juce::roundToInt(g_utilitiesToggleRowHeight * scale));
    const int toggleWidth = juce::roundToInt(g_utilitiesToggleWidth * scale);
    const int toggleGap = juce::roundToInt(g_utilitiesToggleGap * scale);
    const int toggleMonitorGap = juce::roundToInt(g_utilitiesToggleMonitorGap * scale);
    const int monitorWidth = juce::jmin(juce::roundToInt(g_monitorBoxWidth * scale), toggleRow.getWidth());
    const int totalToggleGroupWidth = 3 * toggleWidth + 2 * toggleGap + toggleMonitorGap + monitorWidth;
    auto toggleGroup = toggleRow.withSizeKeepingCentre(totalToggleGroupWidth, toggleRow.getHeight());

    m_swapLRButton.setBounds(toggleGroup.removeFromLeft(toggleWidth));
    toggleGroup.removeFromLeft(toggleGap);
    m_invertLButton.setBounds(toggleGroup.removeFromLeft(toggleWidth));
    toggleGroup.removeFromLeft(toggleGap);
    m_invertRButton.setBounds(toggleGroup.removeFromLeft(toggleWidth));
    toggleGroup.removeFromLeft(toggleMonitorGap);
    m_monitorModeBox.setBounds(toggleGroup);
}
