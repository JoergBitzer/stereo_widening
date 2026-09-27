#include "ChorusPlayground.h"
#include "../PluginSettings.h"
#include "../algorithms/ChorusDoubler.h"

ChorusPlayground::ChorusPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm)
    : AlgorithmPlayground(apvts)
{
    using CD = ChorusDoubler;
    const auto specs = algorithm.getParamSpecs();
    for (const auto& spec : specs)
    {
        m_knobs.push_back(std::make_unique<PlaygroundKnob>(apvts, spec));
        addAndMakeVisible(*m_knobs.back());
    }
    m_view = std::make_unique<DelayModulationView>(getParameter(specs[CD::kWidth].id), getParameter(specs[CD::kAmount].id),
                                                   getParameter(specs[CD::kDepth].id), getParameter(specs[CD::kRate].id));
    addAndMakeVisible(*m_view);
}

void ChorusPlayground::resized()
{
    const int textBoxWidth = scaled(g_smallKnobTextBoxWidth);
    for (auto& knob : m_knobs)
        knob->setKnobLayout(scaled(g_smallKnobSize), scaled(g_smallKnobLabelHeight), textBoxWidth);
    m_view->setScaleFactor(m_scale);

    // Display across the full width on top, the four knobs in one row below.
    auto area = getLocalBounds();
    const int count = (int) m_knobs.size();
    const int colGap = scaled(g_smallKnobColGap);
    auto knobRow = area.removeFromBottom(m_knobs.front()->getPreferredHeight());
    area.removeFromBottom(scaled(g_smallKnobRowGap));
    m_view->setBounds(area);

    knobRow = knobRow.withSizeKeepingCentre(count * textBoxWidth + (count - 1) * colGap, knobRow.getHeight());
    for (auto& knob : m_knobs)
    {
        knob->setBounds(knobRow.removeFromLeft(textBoxWidth));
        knobRow.removeFromLeft(colGap);
    }
}
