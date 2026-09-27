#include "CombPlayground.h"
#include "../PluginSettings.h"
#include "../algorithms/ComplementaryComb.h"

CombPlayground::CombPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm)
    : AlgorithmPlayground(apvts), m_graph(-24.0f, 12.0f)
{
    const auto specs = algorithm.getParamSpecs();
    for (const auto& spec : specs)
    {
        m_knobs.push_back(std::make_unique<PlaygroundKnob>(apvts, spec));
        addAndMakeVisible(*m_knobs.back());
    }

    // Linear 0-2 kHz, not the usual log axis: the comb's teeth are evenly spaced in Hz
    // (1/Delay apart), so on a linear axis L's peaks visibly sit in R's notches and
    // Delay visibly changes their spacing -- on a log axis they crowd into a solid band.
    m_graph.setLinearAxis(2000.0f);
    m_graph.addCurveRange("L", [this](float lo, float hi) { return channelGainDbRange(true, lo, hi); });
    m_graph.addCurveRange("R", [this](float lo, float hi) { return channelGainDbRange(false, lo, hi); });
    m_graph.setReferenceLine(0.0f, "mono input: 0 dB");
    m_graph.addMarker(getParameter(specs[ComplementaryComb::kCrossover].id), "Crossover");
    addAndMakeVisible(m_graph);

    for (size_t i = 0; i < specs.size(); ++i)
        watchParameter(specs[i].id, [this, i](float value)
        {
            m_values[i] = value;
            m_graph.repaint();
        });
}

juce::Range<float> CombPlayground::channelGainDbRange(bool left, float loHz, float hiHz) const
{
    // Sampled within the pixel column, so the (sharp) notches show their real depth.
    constexpr int numSamples = 16;
    juce::Range<float> range;
    for (int k = 0; k <= numSamples; ++k)
    {
        float leftDb = 0.0f, rightDb = 0.0f;
        ComplementaryComb::monoInputGainDb(loHz + (hiHz - loHz) * (float) k / numSamples, m_values, kDisplaySampleRate, leftDb, rightDb);
        const float db = left ? leftDb : rightDb;
        range = k == 0 ? juce::Range<float>(db, db) : range.getUnionWith(db);
    }
    return range;
}

void CombPlayground::resized()
{
    const int textBoxWidth = scaled(g_smallKnobTextBoxWidth);
    for (auto& knob : m_knobs)
        knob->setKnobLayout(scaled(g_smallKnobSize), scaled(g_smallKnobLabelHeight), textBoxWidth);
    m_graph.setScaleFactor(m_scale);

    // Graph across the full width on top, the four knobs in one row below.
    auto area = getLocalBounds();
    const int count = (int) m_knobs.size();
    const int colGap = scaled(g_smallKnobColGap);
    auto knobRow = area.removeFromBottom(m_knobs.front()->getPreferredHeight());
    area.removeFromBottom(scaled(g_smallKnobRowGap));
    m_graph.setBounds(area);

    knobRow = knobRow.withSizeKeepingCentre(count * textBoxWidth + (count - 1) * colGap, knobRow.getHeight());
    for (auto& knob : m_knobs)
    {
        knob->setBounds(knobRow.removeFromLeft(textBoxWidth));
        knobRow.removeFromLeft(colGap);
    }
}
