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
    m_algorithms.push_back(std::make_unique<MultibandWidth>());
    m_algorithms.push_back(std::make_unique<EarlyReflections>());
    m_algorithms.push_back(std::make_unique<ChorusDoubler>());

    // user-configurable defaults (plan2.md Phase 4, "Global settings file"), previously
    // fixed compiled-in constants -- see GlobalSettings.h
    if (auto* comb = dynamic_cast<ComplementaryComb*>(m_algorithms[2].get()))
        comb->setCrossoverHz(m_globalSettings.getCombCrossoverHz());
    if (auto* earlyRefl = dynamic_cast<EarlyReflections*>(m_algorithms[5].get()))
        earlyRefl->setPreDelayMs(m_globalSettings.getEarlyReflectionsPreDelayMs());
    if (auto* chorus = dynamic_cast<ChorusDoubler*>(m_algorithms[6].get()))
        chorus->setRateHz(m_globalSettings.getChorusRateHz());
}

AlgorithmParamValues StereoWidenerAudio::valuesFor(int algorithmIndex) const noexcept
{
    AlgorithmParamValues values {};
    const auto& params = m_algorithmParams[(size_t) algorithmIndex];
    for (size_t i = 0; i < params.size(); ++i)
        values[i] = params[i]->get();
    return values;
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

        m_algorithms[(size_t) m_activeIndex]->process(buffer, valuesFor(m_activeIndex));
        m_algorithms[(size_t) m_targetIndex]->process(m_crossfadeScratch, valuesFor(m_targetIndex));

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
        m_algorithms[(size_t) m_activeIndex]->process(buffer, valuesFor(m_activeIndex));
    }

    // Utilities (Phase 4 step 2, planing.md 2.13 + 2.2): applied once, after whichever
    // width algorithm just ran, regardless of which one is active -- see
    // UtilityProcessor.h.
    UtilityParams utilityParams;
    utilityParams.rotationDeg = m_rotationParam != nullptr ? m_rotationParam->get() : 0.0f;
    utilityParams.balance = m_balanceParam != nullptr ? m_balanceParam->get() * 0.01f : 0.0f; // % -> -1..1
    utilityParams.gainDb = m_gainParam != nullptr ? m_gainParam->get() : 0.0f;
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

    // Like makeFloatParameter, but takes an explicit step (p.stepSize) instead of
    // deriving one from numDecimalPlaces -- needed for Gain's 0.5 dB increments (not a
    // power of ten, so 10^-numDecimalPlaces can't express it). The displayed decimal
    // count still comes out right on its own: AudioParameterFloat's default text
    // formatting derives it from the NormalisableRange's own interval, not from
    // numDecimalPlaces (which this parameter doesn't even need to declare correctly).
    template <typename ParamDef>
    std::unique_ptr<juce::AudioParameterFloat> makeFloatParameterWithStep(const ParamDef& p, float defaultValue)
    {
        defaultValue = juce::jlimit(p.minValue, p.maxValue, defaultValue);
        return std::make_unique<juce::AudioParameterFloat>(p.ID, p.name,
            juce::NormalisableRange<float>(p.minValue, p.maxValue, p.stepSize, p.skew),
            defaultValue,
            juce::AudioParameterFloatAttributes().withLabel(p.unitName));
    }

    // A true logarithmic range (equal frequency ratios get equal knob rotation), for
    // parameters spanning several octaves, where a linear or power-law skewed mapping
    // would cram the musically useful low end into a sliver of the knob.
    juce::NormalisableRange<float> makeLogFrequencyRange(float minHz, float maxHz)
    {
        return juce::NormalisableRange<float>(minHz, maxHz,
            [](float rangeStart, float rangeEnd, float normalised) // convertFrom0To1
            {
                return rangeStart * std::pow(rangeEnd / rangeStart, normalised);
            },
            [](float rangeStart, float rangeEnd, float value) // convertTo0To1
            {
                return std::log(value / rangeStart) / std::log(rangeEnd / rangeStart);
            },
            [](float rangeStart, float rangeEnd, float value) // snapToLegalValue: clamp, then whole Hz steps
            {
                // Must clamp into [rangeStart, rangeEnd] here, not just round -- this is
                // the ONLY thing standing between an out-of-domain value (e.g. a stale
                // Slider value formatted before a SliderAttachment has synced the real
                // one) and convertTo0To1's std::log(value/rangeStart) above, which is
                // unconditional and produces a result outside [0,1] for any value
                // outside the range, tripping NormalisableRange::clampTo0To1's
                // assertion (fatal as SIGTRAP under a debugger, e.g. pluginval's
                // "Editor Automation" test). JUCE's own default snapToLegalValue always
                // clamps first; a custom one must do the same.
                return std::round(juce::jlimit(rangeStart, rangeEnd, value));
            });
    }

    // One APVTS parameter from an algorithm's AlgorithmParamSpec. The displayed text
    // includes the unit ("150 Hz", "10.0 ms", "100 %") or reads "Off" inside an Off
    // zone, so every host's generic view and the playground knobs (which show the
    // parameter's own text) agree, and no GUI code has to add unit suffixes itself.
    std::unique_ptr<juce::AudioParameterFloat> makeAlgorithmParameter(const AlgorithmParamSpec& spec)
    {
        const auto range = spec.scale == AlgorithmParamSpec::Scale::LogFrequency
            ? makeLogFrequencyRange(spec.minValue, spec.maxValue)
            : juce::NormalisableRange<float>(spec.minValue, spec.maxValue, std::pow(10.0f, (float) -spec.numDecimalPlaces));

        const juce::String unit(spec.unit);
        const int decimals = spec.numDecimalPlaces;
        const float offBelow = spec.offBelow;
        const float offAbove = spec.offAbove;
        const float minValue = spec.minValue;
        const float maxValue = spec.maxValue;

        auto attributes = juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([unit, decimals, offBelow, offAbove](float value, int maxLength) -> juce::String
            {
                juce::String text;
                if (value < offBelow || value > offAbove)
                    text = "Off";
                else
                {
                    text = decimals == 0 ? juce::String(juce::roundToInt(value)) : juce::String(value, decimals);
                    if (unit.isNotEmpty())
                        text << " " << unit;
                }
                return maxLength > 0 ? text.substring(0, maxLength) : text;
            })
            .withValueFromStringFunction([offBelow, offAbove, minValue, maxValue](const juce::String& text) -> float
            {
                if (text.trim().equalsIgnoreCase("off"))
                    return offBelow > minValue ? minValue : (offAbove < maxValue ? maxValue : minValue);
                return text.getFloatValue();
            });

        return std::make_unique<juce::AudioParameterFloat>(spec.id, spec.name, range,
            juce::jlimit(spec.minValue, spec.maxValue, spec.defaultValue), attributes);
    }
}

void StereoWidenerAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>> &paramVector)
{
    // Every parameter's default is its own compiled-in default (the algorithm's
    // AlgorithmParamSpec::defaultValue, or the g_param* struct for the utilities), chosen
    // to be as close to neutral (unchanged/pass-through) processing as possible for its
    // algorithm -- this is also the value a double-click on the GUI knob resets to
    // (JUCE's SliderParameterAttachment wires that up automatically from the
    // parameter's own default). A brand new instance therefore always starts neutral;
    // a DAW project's own saved state, restored afterwards via setStateInformation(),
    // still overrides this as usual. Previously these defaults were seeded from a
    // "last used state" recorded in the global settings file (removed: it made
    // double-click reset to whatever was last dialled in rather than neutral, and the
    // init.xml preset already covers "restore my last settings" better -- see
    // GlobalSettings.h).
    for (const auto& algorithm : m_algorithms)
        for (const auto& spec : algorithm->getParamSpecs())
            paramVector.push_back(makeAlgorithmParameter(spec));

    paramVector.push_back(std::make_unique<juce::AudioParameterChoice>(g_paramAlgorithmID, g_paramAlgorithmName,
        g_algorithmNames, 0));

    // Utilities (Phase 4 step 2)
    paramVector.push_back(makeFloatParameter(g_paramRotation, g_paramRotation.defaultValue));
    paramVector.push_back(makeFloatParameter(g_paramBalance, g_paramBalance.defaultValue));
    paramVector.push_back(makeFloatParameterWithStep(g_paramGain, g_paramGain.defaultValue));
    paramVector.push_back(std::make_unique<juce::AudioParameterBool>(g_paramInvertL.ID, g_paramInvertL.name, false));
    paramVector.push_back(std::make_unique<juce::AudioParameterBool>(g_paramInvertR.ID, g_paramInvertR.name, false));
    paramVector.push_back(std::make_unique<juce::AudioParameterBool>(g_paramSwapLR.ID, g_paramSwapLR.name, false));
    paramVector.push_back(std::make_unique<juce::AudioParameterChoice>(g_paramMonitorModeID, g_paramMonitorModeName,
        g_monitorModeNames, 0));
}

void StereoWidenerAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState> &vts)
{
    m_algorithmParams.clear();
    for (const auto& algorithm : m_algorithms)
    {
        std::vector<juce::AudioParameterFloat*> params;
        for (const auto& spec : algorithm->getParamSpecs())
        {
            auto* param = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(spec.id));
            jassert(param != nullptr); // every spec was registered by addParameter()
            params.push_back(param);
        }
        jassert(params.size() <= (size_t) kMaxAlgorithmParams);
        m_algorithmParams.push_back(std::move(params));
    }
    m_algorithmParam = dynamic_cast<juce::AudioParameterChoice*>(vts->getParameter(g_paramAlgorithmID));

    m_rotationParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramRotation.ID));
    m_balanceParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramBalance.ID));
    m_gainParam = dynamic_cast<juce::AudioParameterFloat*>(vts->getParameter(g_paramGain.ID));
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

    addAndMakeVisible(m_levelMeterIn);
    addAndMakeVisible(m_goniometer);
    addAndMakeVisible(m_levelMeterOut);

    // Build/version footer, anchored to the bottom of the whole plugin window in both
    // themes (see m_footerLabel's own comment) -- previously drawn inside the
    // goniometer's own corner (same text StereoAnalyzer's goniometer still shows).
    const juce::String versionText = "v" + juce::String(PLUGIN_VERSION_MAJOR) + "."
                                    + juce::String(PLUGIN_VERSION_MINOR) + "." + juce::String(PLUGIN_VERSION_PATCH);
    m_footerLabel.setText("Built at Jade Hochschule Oldenburg - " + versionText, juce::dontSendNotification);
    m_footerLabel.setJustificationType(juce::Justification::centred);
    m_footerLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    addAndMakeVisible(m_footerLabel);

    // One playground per algorithm, all created up front and bound permanently to
    // their own parameters; showPlaygroundForSelectedAlgorithm() only toggles which one
    // is visible.
    for (int i = 0; i < m_processor.m_algo.getNumAlgorithms(); ++i)
    {
        m_playgrounds.push_back(createPlayground(m_apvts, m_processor.m_algo.getAlgorithm(i)));
        addChildComponent(*m_playgrounds.back()); // starts invisible, unlike addAndMakeVisible
    }

    m_helpButton.onClick = [this] { showAlgorithmHelp(); };
    addAndMakeVisible(m_helpButton);

    for (int i = 0; i < g_algorithmNames.size(); ++i)
        m_algorithmBox.addItem(g_algorithmNames[i], i + 1); // JUCE ComboBox item IDs are 1-based
    addAndMakeVisible(m_algorithmBox);
    m_algorithmBox.onChange = [this] { showPlaygroundForSelectedAlgorithm(); };
    m_algorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        m_apvts, g_paramAlgorithmID, m_algorithmBox);

    m_monoSafeBadge.setJustificationType(juce::Justification::centred);
    m_monoSafeBadge.setColour(juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible(m_monoSafeBadge);

    showPlaygroundForSelectedAlgorithm(); // onChange above only fires on a later *change*, not this initial state

    // Utilities (Phase 4 step 2), applied regardless of the selected algorithm -- see
    // UtilityProcessor.h. Stacked below the output meter (Phase 5 GUI compaction), not
    // labelled as a group any more -- their position already says "this acts on the
    // output"; the two captions below instead name the specific sub-groups that lacked
    // any text of their own (the toggle buttons, the Monitor selector).
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

    m_gainLabel.setText("Gain", juce::dontSendNotification);
    m_gainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_gainLabel);
    m_gainKnob.setTextValueSuffix(" dB");
    addAndMakeVisible(m_gainKnob);
    m_gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        m_apvts, g_paramGain.ID, m_gainKnob);

    m_toggleCaption.setText("Flip", juce::dontSendNotification);
    m_toggleCaption.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_toggleCaption);

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

    m_monitorLabel.setText("Monitor", juce::dontSendNotification);
    m_monitorLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_monitorLabel);

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

void StereoWidenerGUI::showPlaygroundForSelectedAlgorithm()
{
    const int index = juce::jlimit(0, m_processor.m_algo.getNumAlgorithms() - 1, m_algorithmBox.getSelectedItemIndex());
    for (size_t i = 0; i < m_playgrounds.size(); ++i)
        m_playgrounds[i]->setVisible((int) i == index);

    // "Not mono-safe" badge (plan2.md Phase 5 step 4): empty (but still laid out, see
    // resized()) for every mono-safe algorithm.
    m_monoSafeBadge.setText(m_processor.m_algo.getAlgorithm(index).isMonoSafe() ? juce::String()
        : juce::String::fromUTF8("\xe2\x9a\xa0 Not mono-safe -- check Utilities \xe2\x86\x92 Monitor \xe2\x86\x92 Mono Check"),
        juce::dontSendNotification);
}

int StereoWidenerGUI::getRequiredContentHeight() noexcept
{
    // Mirrors resized()'s own layout math at scale = 1.0 -- see the constants there
    // (PluginSettings.h) for what each term is.
    const int paramContentHeight = g_playgroundHeight + g_rowGap + g_monoSafeBadgeHeight;
    const int utilContentHeight = (g_utilKnobLabelHeight + g_utilKnobSize + g_utilKnobLabelHeight) + g_rowGap
                                 + g_utilCaptionHeight + g_rowGap + g_utilToggleRowHeight + g_rowGap
                                 + g_utilCaptionHeight + g_rowGap + g_utilMonitorBoxHeight;
    const int panelsHeight = juce::jmax(paramContentHeight, utilContentHeight) + 2 * g_panelPadding;

    return g_meterRowHeight + g_rowGap + g_algorithmRowHeight + g_rowGap + panelsHeight + g_rowGap + g_footerHeight;
}

void StereoWidenerGUI::paint(juce::Graphics &g)
{
    // Plain ambient background, matching PluginEditor's own top bar exactly (no
    // brightening offset any more): the day/night theme now chooses this colour
    // deliberately per mode, including a Night background darkened specifically so it
    // reads as one continuous surface with the (always-black) meter panels rather
    // than two visibly different shades -- see PluginLookAndFeel.h/.cpp.
    const auto background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    g.fillAll(background);

    // The parameter panel (left two-thirds) and Utilities panel (right third): a
    // "card" background behind each, drawn here (so it sits behind every child
    // component, painted afterwards in the usual z-order) at the bounds resized()
    // computed. Direction (brighter vs. darker) follows the background's own
    // perceived brightness rather than always brightening: Day's background is
    // already close to white (~0.95), so "brighter" has almost no headroom left --
    // tried first, and found (via the offline GUI render, not assumed) to make the
    // panels -- and so the new divider line between them, below -- nearly invisible
    // in Day mode specifically, while working fine in Night mode's near-black
    // background. Darkening a light background (or brightening a dark one) always has
    // real headroom to work with, so this stays visibly a "card" in either theme.
    const bool backgroundIsLight = background.getPerceivedBrightness() > 0.5f;
    const juce::Colour panelColour = backgroundIsLight ? background.darker(0.06f) : background.brighter(0.08f);
    g.setColour(panelColour);
    const float corner = (float) juce::roundToInt(g_panelCornerSize * m_processor.getScaleFactor());
    g.fillRoundedRectangle(m_paramPanelBounds.toFloat(), corner);
    g.fillRoundedRectangle(m_utilPanelBounds.toFloat(), corner);
}

void StereoWidenerGUI::resized()
{
    const float scale = m_processor.getScaleFactor();
    m_levelMeterIn.setScaleFactor(scale);
    m_goniometer.setScaleFactor(scale);
    m_levelMeterOut.setScaleFactor(scale);

    auto r = getLocalBounds();
    const int rowGap = juce::roundToInt(g_rowGap * scale);

    // Footer: reserved from the bottom FIRST, so it always sits at the very bottom of
    // the window.
    const int footerHeight = juce::roundToInt(g_footerHeight * scale);
    m_footerLabel.setBounds(r.removeFromBottom(footerHeight));
    r.removeFromBottom(rowGap);

    // top row: input level meter | goniometer (in/out overlaid) | output level meter,
    // all exactly the same height (goniometer's own vertical padding matches the level
    // meters' horizontal one, so it is no longer "a little smaller" than them).
    auto meterRow = r.removeFromTop(juce::roundToInt(g_meterRowHeight * scale));
    r.removeFromTop(rowGap);

    const int levelWidth = juce::roundToInt(g_levelMeterWidth * scale);
    const int levelPadding = juce::roundToInt(g_levelMeterPadding * scale);
    const int goniometerPaddingX = juce::roundToInt(g_goniometerPadding * scale);
    m_levelMeterIn.setBounds(meterRow.removeFromLeft(levelWidth).reduced(levelPadding));
    m_levelMeterOut.setBounds(meterRow.removeFromRight(levelWidth).reduced(levelPadding));
    m_goniometer.setBounds(meterRow.reduced(goniometerPaddingX, levelPadding));

    // Shared left-two-thirds/right-third split used by both the algorithm-selector row
    // and the two boxed panels below -- g_rightBlockWidth, NOT levelWidth: the meter
    // row uses quarters (input | goniometer x2 | output) while this and everything
    // below it uses thirds, so the two are deliberately independent constants now (see
    // g_levelMeterWidth's own comment).
    const int rightBlockWidth = juce::roundToInt(g_rightBlockWidth * scale);
    const int leftBlockWidth = getWidth() - rightBlockWidth;

    // Algorithm selector row: centred within the left two-thirds. "?" help button to
    // the right of the combo box (used to be to its left), exactly as tall as it (both
    // derive from algoRowHeight, so there is nothing to keep in sync by hand).
    const int algoRowHeight = juce::roundToInt(g_algorithmRowHeight * scale);
    const int helpGap = juce::roundToInt(g_helpButtonGap * scale);
    auto algoRow = r.removeFromTop(algoRowHeight);
    r.removeFromTop(rowGap);

    auto algoBlock = algoRow.removeFromLeft(leftBlockWidth);
    const int boxWidth = juce::jmin(juce::roundToInt(g_algorithmBoxWidth * scale),
                                     algoBlock.getWidth() - algoRowHeight - helpGap);
    auto algoGroup = algoBlock.withSizeKeepingCentre(boxWidth + helpGap + algoRowHeight, algoRowHeight);
    m_algorithmBox.setBounds(algoGroup.removeFromLeft(boxWidth));
    algoGroup.removeFromLeft(helpGap);
    m_helpButton.setBounds(algoGroup); // whatever's left = exactly algoRowHeight square

    // Two boxed "card" panels side by side (drawn in paint(), from m_paramPanelBounds/
    // m_utilPanelBounds set below): left two-thirds = the selected algorithm's
    // playground plus the mono-safe badge; right third = Utilities. Both have the same
    // fixed height for every algorithm. An explicit g_panelDividerWidth gap is removed
    // between the two outer (card-background) rectangles -- a visible thin line in the plain
    // (unbrightened) background colour, per explicit request, rather than leaving them
    // flush against each other (each panel's own g_panelPadding is a separate, inner
    // inset between its background and its own content, not a gap between the panels).
    const int panelPadding = juce::roundToInt(g_panelPadding * scale);
    const int panelDividerWidth = juce::roundToInt(g_panelDividerWidth * scale);

    const int playgroundHeight = juce::roundToInt(g_playgroundHeight * scale);
    const int badgeHeight = juce::roundToInt(g_monoSafeBadgeHeight * scale);

    const int utilKnobSize = juce::roundToInt(g_utilKnobSize * scale);
    const int utilLabelHeight = juce::roundToInt(g_utilKnobLabelHeight * scale);
    const int utilKnobGap = juce::roundToInt(g_utilKnobGap * scale);
    const int utilCaptionHeight = juce::roundToInt(g_utilCaptionHeight * scale);
    const int utilToggleRowHeight = juce::roundToInt(g_utilToggleRowHeight * scale);
    const int utilToggleWidth = juce::roundToInt(g_utilToggleWidth * scale);
    const int utilToggleGap = juce::roundToInt(g_utilToggleGap * scale);
    const int utilMonitorBoxHeight = juce::roundToInt(g_utilMonitorBoxHeight * scale);

    const int paramContentHeight = playgroundHeight + rowGap + badgeHeight;
    const int utilContentHeight = (utilLabelHeight + utilKnobSize + utilLabelHeight) + rowGap
                                 + utilCaptionHeight + rowGap + utilToggleRowHeight + rowGap
                                 + utilCaptionHeight + rowGap + utilMonitorBoxHeight;
    const int contentHeight = juce::jmax(paramContentHeight, utilContentHeight);

    auto panelsRow = r.removeFromTop(contentHeight + 2 * panelPadding);
    m_paramPanelBounds = panelsRow.removeFromLeft(leftBlockWidth - panelDividerWidth);
    panelsRow.removeFromLeft(panelDividerWidth); // the visible dividing line -- see above
    m_utilPanelBounds = panelsRow; // remainder = rightBlockWidth

    auto paramArea = m_paramPanelBounds.reduced(panelPadding);
    auto utilArea = m_utilPanelBounds.reduced(panelPadding);

    // Parameter panel: the "not mono-safe" badge is a strip at the bottom (always
    // reserved, empty text when the active algorithm is mono-safe); the playground
    // gets everything above it -- the same bounds for every algorithm.
    m_monoSafeBadge.setBounds(paramArea.removeFromBottom(badgeHeight));
    paramArea.removeFromBottom(rowGap);
    for (auto& playground : m_playgrounds)
    {
        playground->setBounds(paramArea);
        playground->setScaleFactor(scale);
    }

    // Utilities panel (Phase 4 step 2, its own boxed card since Phase 6's GUI redesign
    // -- every utility acts on the final output signal, see UtilityProcessor.h) --
    // Rotation/Balance knobs, a caption, the toggle buttons, another caption, then the
    // Monitor selector, all sharing utilArea's own width. Vertically centred within
    // utilArea's actual height (contentHeight, which can be taller than
    // utilContentHeight itself if the parameter panel is the taller of the two) --
    // horizontally a no-op, since utilArea is already the right width.
    auto utilStack = utilArea.withSizeKeepingCentre(utilArea.getWidth(), utilContentHeight);

    auto utilKnobRow = utilStack.removeFromTop(utilLabelHeight + utilKnobSize + utilLabelHeight);
    auto utilKnobsCentred = utilKnobRow.withSizeKeepingCentre(3 * utilKnobSize + 2 * utilKnobGap, utilKnobRow.getHeight());
    auto rotationArea = utilKnobsCentred.removeFromLeft(utilKnobSize);
    utilKnobsCentred.removeFromLeft(utilKnobGap);
    auto balanceArea = utilKnobsCentred.removeFromLeft(utilKnobSize);
    utilKnobsCentred.removeFromLeft(utilKnobGap);
    auto gainArea = utilKnobsCentred;

    // Each label spans its knob plus the gap (half on either side), not just the 40 px
    // knob -- "Rotation"/"Balance" don't fit in 40 px and were truncated to "Rota...".
    const auto placeUtilKnob = [&](juce::Rectangle<int> area, juce::Label& label, juce::Slider& knob)
    {
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, utilKnobSize, utilLabelHeight);
        label.setBounds(area.removeFromTop(utilLabelHeight).expanded(utilKnobGap / 2, 0));
        knob.setBounds(area);
    };
    placeUtilKnob(rotationArea, m_rotationLabel, m_rotationKnob);
    placeUtilKnob(balanceArea, m_balanceLabel, m_balanceKnob);
    placeUtilKnob(gainArea, m_gainLabel, m_gainKnob);
    utilStack.removeFromTop(rowGap);

    m_toggleCaption.setBounds(utilStack.removeFromTop(utilCaptionHeight));
    utilStack.removeFromTop(rowGap);

    auto toggleRow = utilStack.removeFromTop(utilToggleRowHeight);
    auto toggleGroup = toggleRow.withSizeKeepingCentre(3 * utilToggleWidth + 2 * utilToggleGap, toggleRow.getHeight());
    m_swapLRButton.setBounds(toggleGroup.removeFromLeft(utilToggleWidth));
    toggleGroup.removeFromLeft(utilToggleGap);
    m_invertLButton.setBounds(toggleGroup.removeFromLeft(utilToggleWidth));
    toggleGroup.removeFromLeft(utilToggleGap);
    m_invertRButton.setBounds(toggleGroup.removeFromLeft(utilToggleWidth));
    utilStack.removeFromTop(rowGap);

    m_monitorLabel.setBounds(utilStack.removeFromTop(utilCaptionHeight));
    utilStack.removeFromTop(rowGap);
    m_monitorModeBox.setBounds(utilStack.removeFromTop(utilMonitorBoxHeight));
}
