#include <math.h>
#include "StereoAnalyzer.h"
#include "SettingsPanel.h"

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

    m_lastIntegration = m_integrationParam != nullptr ? m_integrationParam->get() : g_paramIntegration.defaultValue;
    m_lastPeakHold = m_peakHoldParam != nullptr ? m_peakHoldParam->get() : g_paramPeakHold.defaultValue;
    m_lastPeakDecay = m_peakDecayParam != nullptr ? m_peakDecayParam->get() : g_paramPeakDecay.defaultValue;
    m_meterState.prepare(sampleRate, m_lastIntegration, m_lastPeakDecay, m_lastPeakHold);
}

namespace
{
    bool hasChanged(float value, float lastValue) noexcept
    {
        return std::abs(value - lastValue) > 1.0e-6f;
    }
}

int StereoAnalyzerAudio::processSynchronBlock(juce::AudioBuffer<float> & buffer, juce::MidiBuffer &midiMessages, int NrOfBlocksSinceLastProcessBlock)
{
    juce::ignoreUnused(midiMessages, NrOfBlocksSinceLastProcessBlock);

    // Apply changed settings. A changed Integration time re-prepares the meter state,
    // which resets the running RMS/correlation state -- expected when the user changes
    // that ballistic (comparable to switching ballistics on a hardware meter). Peak hold
    // and peak decay use the lighter setters instead: they only affect the peak meter,
    // so there is no reason to also blank out the RMS/correlation readout.
    if (m_integrationParam != nullptr)
    {
        const float value = m_integrationParam->get();
        if (hasChanged(value, m_lastIntegration))
        {
            m_meterState.prepare(m_sampleRate, value, m_lastPeakDecay, m_lastPeakHold);
            m_lastIntegration = value;
        }
    }
    if (m_peakHoldParam != nullptr)
    {
        const float value = m_peakHoldParam->get();
        if (hasChanged(value, m_lastPeakHold))
        {
            m_meterState.setPeakHoldTime(value);
            m_lastPeakHold = value;
        }
    }
    if (m_peakDecayParam != nullptr)
    {
        const float value = m_peakDecayParam->get();
        if (hasChanged(value, m_lastPeakDecay))
        {
            m_meterState.setPeakDecayRate(value);
            m_lastPeakDecay = value;
        }
    }

    m_meterState.processBlock(buffer);
    // the analyzer is a pure observer: the buffer passes through unchanged
    return 0;
}

namespace
{
    // Builds a float parameter. The slider step (interval) is set to
    // 10^-numDecimalPlaces, e.g. 0.01 for numDecimalPlaces = 2: this both limits how
    // finely the value can be dragged and, because JUCE derives a parameter's displayed
    // decimal count from its interval, gives a clean "0.30 s" instead of "0.300000 s" in
    // our own SettingsPanel slider and in any host's generic parameter/automation view.
    template <typename ParamDef>
    std::unique_ptr<juce::AudioParameterFloat> makeFloatParameter(const ParamDef& p)
    {
        const float interval = std::pow(10.0f, (float) -p.numDecimalPlaces);
        return std::make_unique<juce::AudioParameterFloat>(p.ID, p.name,
            juce::NormalisableRange<float>(p.minValue, p.maxValue, interval, p.skew),
            p.defaultValue,
            juce::AudioParameterFloatAttributes().withLabel(p.unitName));
    }
}

void StereoAnalyzerAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>> &paramVector)
{
    paramVector.push_back(makeFloatParameter(g_paramIntegration));
    paramVector.push_back(makeFloatParameter(g_paramPeakHold));
    paramVector.push_back(makeFloatParameter(g_paramPeakDecay));
}

void StereoAnalyzerAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState> &vts)
{
    m_integrationParam = dynamic_cast<AudioParameterFloat*>(vts->getParameter(g_paramIntegration.ID));
    m_peakHoldParam = dynamic_cast<AudioParameterFloat*>(vts->getParameter(g_paramPeakHold.ID));
    m_peakDecayParam = dynamic_cast<AudioParameterFloat*>(vts->getParameter(g_paramPeakDecay.ID));
}


namespace
{
    void drawGearIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        const auto centre = bounds.getCentre();
        const float outerRadius = 0.5f * juce::jmin(bounds.getWidth(), bounds.getHeight());
        const float rootRadius = outerRadius * 0.72f;  // between teeth
        const float innerRadius = outerRadius * 0.42f; // centre hole
        constexpr int numTeeth = 8;

        juce::Path gear;
        for (int i = 0; i < numTeeth * 2; ++i)
        {
            const float angle = juce::MathConstants<float>::twoPi * (float) i / (float) (numTeeth * 2);
            const float r = (i % 2 == 0) ? outerRadius : rootRadius;
            const auto point = centre.getPointOnCircumference(r, angle);
            if (i == 0)
                gear.startNewSubPath(point);
            else
                gear.lineTo(point);
        }
        gear.closeSubPath();
        gear.addEllipse(centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);
        gear.setUsingNonZeroWinding(false); // even-odd fill: the ellipse subpath cuts a hole

        g.setColour(colour);
        g.fillPath(gear);
    }
}

void GearButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat();

    auto background = findColour(juce::TextButton::buttonColourId);
    if (isButtonDown)
        background = background.brighter(0.3f);
    else if (isMouseOverButton)
        background = background.brighter(0.15f);
    g.setColour(background);
    g.fillRoundedRectangle(bounds, 0.15f * juce::jmin(bounds.getWidth(), bounds.getHeight()));

    drawGearIcon(g, bounds.reduced(bounds.getWidth() * 0.18f), findColour(juce::TextButton::textColourOffId));
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

    m_settingsButton.onClick = [this] { showSettings(); };
    addAndMakeVisible(m_settingsButton);

    // fixed, not user-adjustable -- see the comment on kGoniometerAfterglowSeconds
    m_goniometer.setAfterglowTime(kGoniometerAfterglowSeconds);

    // the goniometer's bottom-left corner stays empty of grid/points (especially now
    // that overload points are clamped onto the circle instead), so it is a natural spot
    // for a quiet build/version footnote. A logo image was tried here first, but its
    // raster artwork did not scale well to this corner's size -- plain text renders
    // crisply at any size instead (see docs/algorithms/phase2_stereo_analyzer.md).
    const juce::String versionText = "Version: " + juce::String(PLUGIN_VERSION_MAJOR) + "."
                                    + juce::String(PLUGIN_VERSION_MINOR) + "." + juce::String(PLUGIN_VERSION_PATCH);
    m_goniometer.setCornerText({ "Built at Jade Hochschule Oldenburg", versionText });
}

void StereoAnalyzerGUI::showSettings()
{
    auto panel = std::make_unique<SettingsPanel>(m_apvts);
    juce::CallOutBox::launchAsynchronously(std::move(panel), m_settingsButton.getScreenBounds(), nullptr);
}

void StereoAnalyzerGUI::paint(juce::Graphics &g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).brighter(0.3f));
}

void StereoAnalyzerGUI::resized()
{
    // dragging the plugin's corner resizes this component; propagate the same scale
    // factor the template already uses for the preset handler/MIDI keyboard (see
    // PluginEditor.cpp) to the meters, so their fonts and other pixel-unit details
    // scale too, and scale the row sizes below the same way so nothing overflows them
    const float scale = m_processor.getScaleFactor();
    m_goniometer.setScaleFactor(scale);
    m_correlationMeter.setScaleFactor(scale);
    m_levelMeter.setScaleFactor(scale);

	auto r = getLocalBounds();

    auto correlationRow = r.removeFromBottom(juce::roundToInt(g_correlationRowHeight * scale));
    m_correlationMeter.setBounds(correlationRow.reduced(juce::roundToInt(g_correlationPaddingX * scale),
                                                          juce::roundToInt(g_correlationPaddingY * scale)));

    const int levelWidth = juce::jmin(juce::roundToInt(g_levelMeterMaxWidth * scale), r.getWidth() / g_levelMeterWidthDivisor);
    auto levelBounds = r.removeFromRight(levelWidth).reduced(juce::roundToInt(g_levelMeterPadding * scale));
    m_levelMeter.setBounds(levelBounds);
    m_goniometer.setBounds(r.reduced(juce::roundToInt(g_goniometerPadding * scale)));

    // the gear button shares the Levels panel's own top-left corner (to the left of its
    // centred "Levels" title) instead of a dedicated row, freeing that space for the meters
    const int buttonSize = juce::roundToInt(g_settingsButtonSize * scale);
    const int buttonMargin = juce::roundToInt(g_settingsButtonMargin * scale);
    m_settingsButton.setBounds(levelBounds.getX() + buttonMargin, levelBounds.getY() + buttonMargin,
                                buttonSize, buttonSize);
}
