#include "FilteredPlayground.h"
#include "../PluginSettings.h"
#include "../algorithms/MSWidthFiltered.h"

FilteredPlayground::FilteredPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm)
    : AlgorithmPlayground(apvts), m_graph(-24.0f, 12.0f)
{
    const auto specs = algorithm.getParamSpecs();
    for (const auto& spec : specs)
    {
        m_knobs.push_back(std::make_unique<PlaygroundKnob>(apvts, spec));
        addAndMakeVisible(*m_knobs.back());
    }

    m_graph.setCurveName("Side gain");
    m_graph.setReferenceLine(0.0f, "Mid: 0 dB");
    m_graph.setCurve([this](float hz) { return MSWidthFiltered::sideGainDb(hz, m_values, kDisplaySampleRate); });
    m_graph.addHandle(getParameter(specs[MSWidthFiltered::kBassCutoff].id));
    m_graph.addHandle(getParameter(specs[MSWidthFiltered::kHighShelf].id), &getParameter(specs[MSWidthFiltered::kShelfGain].id));
    addAndMakeVisible(m_graph);

    for (size_t i = 0; i < specs.size(); ++i)
        watchParameter(specs[i].id, [this, i](float value)
        {
            m_values[i] = value;
            m_graph.repaint();
        });
}

void FilteredPlayground::resized()
{
    const int textBoxWidth = scaled(g_gridKnobTextBoxWidth);
    for (auto& knob : m_knobs)
        knob->setKnobLayout(scaled(g_gridKnobSize), scaled(g_gridKnobLabelHeight), textBoxWidth);
    m_graph.setScaleFactor(m_scale);

    // Graph across the full width on top, the four knobs in one row below.
    auto area = getLocalBounds();
    const int count = (int) m_knobs.size();
    const int colGap = scaled(g_gridKnobColGap);
    auto knobRow = area.removeFromBottom(m_knobs.front()->getPreferredHeight());
    area.removeFromBottom(scaled(g_gridKnobRowGap));
    m_graph.setBounds(area);

    knobRow = knobRow.withSizeKeepingCentre(count * textBoxWidth + (count - 1) * colGap, knobRow.getHeight());
    for (auto& knob : m_knobs)
    {
        knob->setBounds(knobRow.removeFromLeft(textBoxWidth));
        knobRow.removeFromLeft(colGap);
    }
}
