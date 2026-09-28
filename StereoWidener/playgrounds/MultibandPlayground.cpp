#include "MultibandPlayground.h"
#include "../PluginSettings.h"
#include "../algorithms/MultibandWidth.h"

MultibandPlayground::MultibandPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm)
    : AlgorithmPlayground(apvts)
{
    using MB = MultibandWidth;
    const auto specs = algorithm.getParamSpecs();

    // Short labels matching the display: split k separates band k and k+1; band k's
    // width knob sits between the knobs of the splits that bound it (see resized()).
    const juce::StringArray labels { "Split 1", "Split 2", "Split 3", "Width 2", "Width 3", "Width 4" };
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

    limitCrossoverKnobs();
}

void MultibandPlayground::limitCrossoverKnobs()
{
    // The display stops a dragged crossover at its neighbours; the knobs do the same,
    // but only for the user's own changes: dragging the knob or typing a value. Changes
    // from the host (automation, presets) are left alone -- writing a parameter back
    // while the host sets it would fight the automation, and could oscillate when the
    // host sets two crossovers closer than allowed; the DSP sorts and separates
    // whatever arrives anyway (MultibandWidth::updateFrequenciesIfNeeded()).
    for (int index : { MultibandWidth::kFreq1, MultibandWidth::kFreq2, MultibandWidth::kFreq3 })
    {
        auto& slider = m_knobs[(size_t) index]->getSlider();
        slider.onValueChange = [this, index, &slider]
        {
            if (!slider.isMouseButtonDown())
                return;
            const double limited = m_bandSplit->limitCrossover(index, (float) slider.getValue());
            if (!juce::approximatelyEqual(limited, slider.getValue()))
                slider.setValue(limited, juce::sendNotificationSync);
        };
        // Typed values: wraps the attachment's own text parser (set up when the
        // knob's attachment was created, so this has to come afterwards).
        auto parse = slider.valueFromTextFunction;
        slider.valueFromTextFunction = [this, index, parse](const juce::String& text)
        {
            return (double) m_bandSplit->limitCrossover(index, (float) parse(text));
        };
    }
}

void MultibandPlayground::resized()
{
    using MB = MultibandWidth;
    const int textBoxWidth = scaled(g_compactKnobTextBoxWidth);
    for (auto& knob : m_knobs)
        knob->setKnobLayout(scaled(g_compactKnobSize), scaled(g_compactKnobLabelHeight), textBoxWidth);
    m_bandSplit->setScaleFactor(m_scale);

    // Display across the full width on top; below it the knobs in two staggered rows,
    // laid out like the frequency axis: the splits (crossovers) in the first row, and
    // each band's width in the second, half a step to the right -- between the two
    // splits that bound the band (band 4: right of split 3). The rows overlap by half a
    // knob, which is free space because the columns alternate.
    const int knobHeight = m_knobs.front()->getPreferredHeight();
    const int rowOffset = scaled(g_compactKnobLabelHeight) + scaled(g_compactKnobSize) / 2;
    auto area = getLocalBounds();
    auto knobArea = area.removeFromBottom(rowOffset + knobHeight);
    area.removeFromBottom(scaled(g_compactKnobGap));
    m_bandSplit->setBounds(area);

    // six columns, alternating split / width; a column step of at least the text box
    // plus a small gap, at most a comfortable spacing
    const int step = juce::jlimit(textBoxWidth + scaled(g_compactKnobGap), textBoxWidth + scaled(g_compactKnobGroupGap),
                                  (knobArea.getWidth() - textBoxWidth) / 5);
    const int left = knobArea.getX() + (knobArea.getWidth() - (5 * step + textBoxWidth)) / 2;
    for (int k = 0; k < 3; ++k)
    {
        m_knobs[(size_t) (MB::kFreq1 + k)]->setBounds(left + 2 * k * step, knobArea.getY(), textBoxWidth, knobHeight);
        m_knobs[(size_t) (MB::kWidth2 + k)]->setBounds(left + (2 * k + 1) * step, knobArea.getY() + rowOffset,
                                                       textBoxWidth, knobHeight);
    }
}
