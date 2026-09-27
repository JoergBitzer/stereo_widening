#include "MultibandPlayground.h"
#include "../PluginSettings.h"
#include "../algorithms/MultibandWidth.h"

MultibandPlayground::MultibandPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm)
    : AlgorithmPlayground(apvts)
{
    using MB = MultibandWidth;
    const auto specs = algorithm.getParamSpecs();

    // Short labels matching the display: split k separates band k and k+1.
    const juce::StringArray labels { "Split 1", "Split 2", "Split 3", "Band 2", "Band 3", "Band 4" };
    for (size_t i = 0; i < specs.size(); ++i)
    {
        m_knobs.push_back(std::make_unique<PlaygroundKnob>(apvts, specs[i], labels[(int) i]));
        addAndMakeVisible(*m_knobs.back());
    }

    m_bandSplit = std::make_unique<BandSplitView>(
        std::array<juce::RangedAudioParameter*, 3> { &getParameter(specs[MB::kFreq1].id), &getParameter(specs[MB::kFreq2].id),
                                                     &getParameter(specs[MB::kFreq3].id) },
        std::array<juce::RangedAudioParameter*, 3> { &getParameter(specs[MB::kWidth2].id), &getParameter(specs[MB::kWidth3].id),
                                                     &getParameter(specs[MB::kWidth4].id) },
        MB::kMinCrossoverRatio);
    addAndMakeVisible(*m_bandSplit);

    // The display stops a dragged crossover at its neighbours; the knobs do the same.
    // Wired only after every knob's attachment has synced its initial value, so no
    // crossover is ever compared against a neighbour that isn't set yet.
    for (int index : { MB::kFreq1, MB::kFreq2, MB::kFreq3 })
        m_knobs[(size_t) index]->getSlider().onValueChange = [this, index] { keepCrossoverKnobBetweenNeighbours(index); };
}

void MultibandPlayground::keepCrossoverKnobBetweenNeighbours(int index)
{
    using MB = MultibandWidth;
    auto& slider = m_knobs[(size_t) index]->getSlider();
    if (index > MB::kFreq1)
    {
        const double lower = m_knobs[(size_t) index - 1]->getSlider().getValue() * MB::kMinCrossoverRatio;
        if (slider.getValue() < lower)
            slider.setValue(lower, juce::sendNotificationSync);
    }
    if (index < MB::kFreq3)
    {
        const double upper = m_knobs[(size_t) index + 1]->getSlider().getValue() / MB::kMinCrossoverRatio;
        if (slider.getValue() > upper)
            slider.setValue(upper, juce::sendNotificationSync);
    }
}

void MultibandPlayground::resized()
{
    const int textBoxWidth = scaled(g_compactKnobTextBoxWidth);
    for (auto& knob : m_knobs)
        knob->setKnobLayout(scaled(g_compactKnobSize), scaled(g_compactKnobLabelHeight), textBoxWidth);
    m_bandSplit->setScaleFactor(m_scale);

    // Display across the full width on top; below it the knobs in two groups:
    // splits (left), band widths (right).
    auto area = getLocalBounds();
    auto knobRow = area.removeFromBottom(m_knobs.front()->getPreferredHeight());
    area.removeFromBottom(scaled(g_smallKnobRowGap));
    m_bandSplit->setBounds(area);

    const int gap = scaled(g_compactKnobGap);
    const int groupGap = scaled(g_compactKnobGroupGap);
    const int rowWidth = 6 * textBoxWidth + 4 * gap + groupGap;
    knobRow = knobRow.withSizeKeepingCentre(rowWidth, knobRow.getHeight());
    for (size_t i = 0; i < m_knobs.size(); ++i)
    {
        m_knobs[i]->setBounds(knobRow.removeFromLeft(textBoxWidth));
        knobRow.removeFromLeft(i == 2 ? groupGap : gap);
    }
}
