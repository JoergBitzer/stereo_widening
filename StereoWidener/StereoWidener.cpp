#include <cmath>
#include "StereoWidener.h"

#include "PluginProcessor.h"

StereoWidenerAudio::StereoWidenerAudio(StereoWidenerAudioProcessor* processor)
:SynchronBlockProcessor(), m_processor(processor)
{
    // must stay in the same order as g_algorithmNames (StereoWidener.h)
    m_algorithms.push_back(std::make_unique<MSWidthBroadband>());
    m_algorithms.push_back(std::make_unique<MSWidthFiltered>());
    m_algorithms.push_back(std::make_unique<ComplementaryComb>());
    m_algorithms.push_back(std::make_unique<AllpassDecorrelation>());

    // user-configurable defaults (plan2.md Phase 4, "Global settings file"), previously
    // fixed compiled-in constants -- see GlobalSettings.h
    if (auto* filtered = dynamic_cast<MSWidthFiltered*>(m_algorithms[1].get()))
        filtered->setHighShelfGainDb(m_globalSettings.getHighShelfGainDb());
    if (auto* comb = dynamic_cast<ComplementaryComb*>(m_algorithms[2].get()))
        comb->setCrossoverHz(m_globalSettings.getCombCrossoverHz());
}

StereoAlgorithmParams StereoWidenerAudio::paramsFor(int algorithmIndex, float width) const noexcept
{
    StereoAlgorithmParams p;
    p.width = width;

    // Which parameters feed auxLeft/auxRight is per-algorithm -- the one place that
    // needs to know about every algorithm's own aux parameters, matching
    // StereoWidenerGUI::auxLeftParamIdFor()/auxRightParamIdFor() (same index order).
    switch (algorithmIndex)
    {
        case 1: // MSWidthFiltered: Bass Cutoff (Hz), High Shelf (Hz)
            p.auxLeft = m_bassCutoffParam != nullptr ? m_bassCutoffParam->get() : g_paramBassCutoff.defaultValue;
            p.auxRight = m_highShelfFreqParam != nullptr ? m_highShelfFreqParam->get() : g_paramHighShelfFreq.defaultValue;
            break;
        case 2: // ComplementaryComb: Delay (ms), Gain (0-100 % -> 0-1)
            p.auxLeft = m_combDelayParam != nullptr ? m_combDelayParam->get() : g_paramCombDelay.defaultValue;
            p.auxRight = (m_combGainParam != nullptr ? m_combGainParam->get() : g_paramCombGain.defaultValue) * 0.01f;
            break;
        case 3: // AllpassDecorrelation: Amount (0-100 % -> 0-1), Spread (0-100 % -> 0-1)
            p.auxLeft = (m_allpassAmountParam != nullptr ? m_allpassAmountParam->get() : g_paramAllpassAmount.defaultValue) * 0.01f;
            p.auxRight = (m_allpassSpreadParam != nullptr ? m_allpassSpreadParam->get() : g_paramAllpassSpread.defaultValue) * 0.01f;
            break;
        default: // MSWidthBroadband and any future algorithm with no aux params
            break;
    }
    return p;
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

        m_algorithms[(size_t) m_activeIndex]->process(buffer, paramsFor(m_activeIndex, width));
        m_algorithms[(size_t) m_targetIndex]->process(m_crossfadeScratch, paramsFor(m_targetIndex, width));

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
        m_algorithms[(size_t) m_activeIndex]->process(buffer, paramsFor(m_activeIndex, width));
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
    // defaultValue is passed explicitly (rather than always using p.defaultValue
    // implicitly) so callers are forced to say plainly which default they mean --
    // currently always p.defaultValue itself, see addParameter(). Still clamped
    // defensively against the parameter's own range.
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
    // Every parameter's default is its own compiled-in g_param*.defaultValue, chosen to
    // be as close to neutral (unchanged/pass-through) processing as possible for its
    // algorithm -- this is also the value a double-click on the GUI knob resets to
    // (JUCE's SliderParameterAttachment wires that up automatically from the
    // parameter's own default). A brand new instance therefore always starts neutral;
    // a DAW project's own saved state, restored afterwards via setStateInformation(),
    // still overrides this as usual. Previously these defaults were seeded from a
    // "last used state" recorded in the global settings file (removed: it made
    // double-click reset to whatever was last dialled in rather than neutral, and the
    // init.xml preset already covers "restore my last settings" better -- see
    // GlobalSettings.h).
    paramVector.push_back(makeFloatParameter(g_paramWidth, g_paramWidth.defaultValue));
    paramVector.push_back(makeFrequencyParameterWithOff(g_paramBassCutoff, MSWidthFiltered::kBassCutoffOffThreshold, true,
        g_paramBassCutoff.defaultValue));
    paramVector.push_back(makeLogFrequencyParameterWithOff(g_paramHighShelfFreq, MSWidthFiltered::kHighShelfOffThreshold,
        g_paramHighShelfFreq.defaultValue));
    paramVector.push_back(makeFloatParameter(g_paramCombDelay, g_paramCombDelay.defaultValue));
    paramVector.push_back(makeFloatParameter(g_paramCombGain, g_paramCombGain.defaultValue));
    paramVector.push_back(makeFloatParameter(g_paramAllpassAmount, g_paramAllpassAmount.defaultValue));
    paramVector.push_back(makeFloatParameter(g_paramAllpassSpread, g_paramAllpassSpread.defaultValue));

    paramVector.push_back(std::make_unique<juce::AudioParameterChoice>(g_paramAlgorithmID, g_paramAlgorithmName,
        g_algorithmNames, 0));

    // Utilities (Phase 4 step 2)
    paramVector.push_back(makeFloatParameter(g_paramRotation, g_paramRotation.defaultValue));
    paramVector.push_back(makeFloatParameter(g_paramBalance, g_paramBalance.defaultValue));
    paramVector.push_back(std::make_unique<juce::AudioParameterBool>(g_paramInvertL.ID, g_paramInvertL.name, false));
    paramVector.push_back(std::make_unique<juce::AudioParameterBool>(g_paramInvertR.ID, g_paramInvertR.name, false));
    paramVector.push_back(std::make_unique<juce::AudioParameterBool>(g_paramSwapLR.ID, g_paramSwapLR.name, false));
    paramVector.push_back(std::make_unique<juce::AudioParameterChoice>(g_paramMonitorModeID, g_paramMonitorModeName,
        g_monitorModeNames, 0));
}

void StereoWidenerAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState> &vts)
{
    m_widthParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramWidth.ID));
    m_bassCutoffParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramBassCutoff.ID));
    m_highShelfFreqParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramHighShelfFreq.ID));
    m_combDelayParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramCombDelay.ID));
    m_combGainParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramCombGain.ID));
    m_allpassAmountParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramAllpassAmount.ID));
    m_allpassSpreadParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramAllpassSpread.ID));
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

    // Widgets only here -- which parameter each one is bound to (and that parameter's
    // own display formatting, e.g. MSWidthFiltered's "Off" zones) depends on the active
    // algorithm and is set up by bindAuxKnob(), called from
    // updateAuxKnobsForActiveAlgorithm() below.
    m_auxLeftLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_auxLeftLabel);
    addAndMakeVisible(m_auxLeftKnob);

    m_auxRightLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_auxRightLabel);
    addAndMakeVisible(m_auxRightKnob);

    m_helpButton.onClick = [this] { showAlgorithmHelp(); };
    addAndMakeVisible(m_helpButton);

    for (int i = 0; i < g_algorithmNames.size(); ++i)
        m_algorithmBox.addItem(g_algorithmNames[i], i + 1); // JUCE ComboBox item IDs are 1-based
    addAndMakeVisible(m_algorithmBox);
    m_algorithmBox.onChange = [this] { updateAuxKnobsForActiveAlgorithm(); };
    m_algorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        m_apvts, g_paramAlgorithmID, m_algorithmBox);

    m_monoSafeBadge.setJustificationType(juce::Justification::centred);
    m_monoSafeBadge.setColour(juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible(m_monoSafeBadge);

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

namespace
{
    // Which APVTS parameter each aux knob should be rebound to for a given algorithm
    // index -- the GUI-side counterpart of StereoWidenerAudio::paramsFor(), and must
    // stay in the same index order as g_algorithmNames/the algorithm instances (see the
    // comment there). Empty string means "this algorithm has no such parameter" --
    // bindAuxKnob() then detaches and disables the knob.
    juce::String auxLeftParamIdFor(int algorithmIndex)
    {
        switch (algorithmIndex)
        {
            case 1: return g_paramBassCutoff.ID;
            case 2: return g_paramCombDelay.ID;
            case 3: return g_paramAllpassAmount.ID;
            default: return {};
        }
    }

    juce::String auxRightParamIdFor(int algorithmIndex)
    {
        switch (algorithmIndex)
        {
            case 1: return g_paramHighShelfFreq.ID;
            case 2: return g_paramCombGain.ID;
            case 3: return g_paramAllpassSpread.ID;
            default: return {};
        }
    }
}

void StereoWidenerGUI::bindAuxKnob(juce::Slider& knob, std::unique_ptr<SliderAttachment>& attachment, const juce::String& paramId)
{
    attachment.reset(); // must be destroyed before a new one is created on the same slider

    knob.textFromValueFunction = nullptr;
    knob.valueFromTextFunction = nullptr;
    knob.setTextValueSuffix({});

    if (paramId.isEmpty())
    {
        // No parameter behind this knob for the active algorithm (e.g.
        // MSWidthBroadband): blank the text box instead of showing the detached
        // slider's own raw numeric value (e.g. "0.0000000"), which would look broken.
        knob.textFromValueFunction = [](double) -> juce::String { return {}; };
        knob.updateText(); // no attachment will run to refresh the cached text box otherwise
        return;
    }

    // Custom text display (not just a unit suffix) for the two parameters with an "Off"
    // zone (see g_paramBassCutoff/g_paramHighShelfFreq); everything else just shows its
    // own unit suffix.
    if (paramId == juce::String(g_paramBassCutoff.ID))
    {
        knob.textFromValueFunction = [](double value) -> juce::String
        {
            return value < MSWidthFiltered::kBassCutoffOffThreshold ? "Off"
                 : juce::String(juce::roundToInt(value)) + " Hz";
        };
        knob.valueFromTextFunction = [](const juce::String& text) -> double
        {
            return text.trim().equalsIgnoreCase("off") ? (double) g_paramBassCutoff.minValue : text.getDoubleValue();
        };
    }
    else if (paramId == juce::String(g_paramHighShelfFreq.ID))
    {
        knob.textFromValueFunction = [](double value) -> juce::String
        {
            return value > MSWidthFiltered::kHighShelfOffThreshold ? "Off"
                 : juce::String(juce::roundToInt(value)) + " Hz";
        };
        knob.valueFromTextFunction = [](const juce::String& text) -> double
        {
            return text.trim().equalsIgnoreCase("off") ? (double) g_paramHighShelfFreq.maxValue : text.getDoubleValue();
        };
    }
    else if (paramId == juce::String(g_paramCombDelay.ID))
    {
        knob.setTextValueSuffix(" ms");
    }
    else if (paramId == juce::String(g_paramCombGain.ID)
             || paramId == juce::String(g_paramAllpassAmount.ID)
             || paramId == juce::String(g_paramAllpassSpread.ID))
    {
        knob.setTextValueSuffix(" %");
    }

    attachment = std::make_unique<SliderAttachment>(m_apvts, paramId, knob);
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
    bindAuxKnob(m_auxLeftKnob, m_auxLeftAttachment, leftInfo.enabled ? auxLeftParamIdFor(index) : juce::String());

    const auto rightInfo = algorithm.getAuxRightInfo();
    m_auxRightKnob.setEnabled(rightInfo.enabled);
    m_auxRightLabel.setText(rightInfo.enabled ? rightInfo.label : juce::String(), juce::dontSendNotification);
    bindAuxKnob(m_auxRightKnob, m_auxRightAttachment, rightInfo.enabled ? auxRightParamIdFor(index) : juce::String());

    // "Not mono-safe" badge (plan2.md Phase 5 step 4): empty (but still laid out, see
    // resized()) for every mono-safe algorithm, so switching algorithms never shifts
    // the Utilities section below it.
    m_monoSafeBadge.setText(algorithm.isMonoSafe() ? juce::String()
        : juce::String::fromUTF8("\xe2\x9a\xa0 Not mono-safe -- check Utilities \xe2\x86\x92 Monitor \xe2\x86\x92 Mono Check"),
        juce::dontSendNotification);
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

    // "Not mono-safe" badge (Phase 5 step 4): always reserved (empty text when the
    // active algorithm is mono-safe), directly below the algorithm row.
    m_monoSafeBadge.setBounds(r.removeFromTop(juce::roundToInt(g_monoSafeBadgeHeight * scale)));
    r.removeFromTop(rowGap);

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
