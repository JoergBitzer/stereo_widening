#include "EarlyReflectionsPlayground.h"
#include "../PluginSettings.h"
#include "../algorithms/EarlyReflections.h"

EarlyReflectionsPlayground::EarlyReflectionsPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm)
    : AlgorithmPlayground(apvts)
{
    using ER = EarlyReflections;
    const auto specs = algorithm.getParamSpecs();
    for (const auto& spec : specs)
    {
        m_knobs.push_back(std::make_unique<PlaygroundKnob>(apvts, spec));
        addAndMakeVisible(*m_knobs.back());
    }
    m_echogram = std::make_unique<EchogramView>(getParameter(specs[ER::kWidth].id), getParameter(specs[ER::kAmount].id),
                                                getParameter(specs[ER::kRoomSize].id), getParameter(specs[ER::kPreDelay].id));
    addAndMakeVisible(*m_echogram);
}

void EarlyReflectionsPlayground::resized()
{
    const int textBoxWidth = scaled(g_smallKnobTextBoxWidth);
    for (auto& knob : m_knobs)
        knob->setKnobLayout(scaled(g_smallKnobSize), scaled(g_smallKnobLabelHeight), textBoxWidth);
    m_echogram->setScaleFactor(m_scale);

    // Echogram across the full width on top, the four knobs in one row below.
    auto area = getLocalBounds();
    const int count = (int) m_knobs.size();
    const int colGap = scaled(g_smallKnobColGap);
    auto knobRow = area.removeFromBottom(m_knobs.front()->getPreferredHeight());
    area.removeFromBottom(scaled(g_smallKnobRowGap));
    m_echogram->setBounds(area);

    knobRow = knobRow.withSizeKeepingCentre(count * textBoxWidth + (count - 1) * colGap, knobRow.getHeight());
    for (auto& knob : m_knobs)
    {
        knob->setBounds(knobRow.removeFromLeft(textBoxWidth));
        knobRow.removeFromLeft(colGap);
    }
}
