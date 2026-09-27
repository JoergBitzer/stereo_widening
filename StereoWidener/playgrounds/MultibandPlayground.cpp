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

    // Group captions, so it's clear the splits are frequencies and the band knobs widths.
    for (auto* caption : { &m_frequencyCaption, &m_widthCaption })
    {
        caption->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*caption);
    }
    m_frequencyCaption.setText("Frequency", juce::dontSendNotification);
    m_widthCaption.setText("Width", juce::dontSendNotification);

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
    const int textBoxWidth = scaled(g_compactKnobTextBoxWidth);
    for (auto& knob : m_knobs)
        knob->setKnobLayout(scaled(g_compactKnobSize), scaled(g_compactKnobLabelHeight), textBoxWidth);
    m_bandSplit->setScaleFactor(m_scale);

    // Display across the full width on top; below it the captions and knobs in two
    // groups: splits (Frequency, left), band widths (Width, right).
    auto area = getLocalBounds();
    auto knobRow = area.removeFromBottom(m_knobs.front()->getPreferredHeight());
    auto captionRow = area.removeFromBottom(scaled(g_compactKnobLabelHeight));
    area.removeFromBottom(scaled(g_smallKnobRowGap));
    m_bandSplit->setBounds(area);

    const int gap = scaled(g_compactKnobGap);
    const int groupGap = scaled(g_compactKnobGroupGap);
    const int groupWidth = 3 * textBoxWidth + 2 * gap;
    knobRow = knobRow.withSizeKeepingCentre(2 * groupWidth + groupGap, knobRow.getHeight());
    captionRow = captionRow.withSizeKeepingCentre(knobRow.getWidth(), captionRow.getHeight());
    m_frequencyCaption.setBounds(captionRow.removeFromLeft(groupWidth));
    m_widthCaption.setBounds(captionRow.removeFromRight(groupWidth));
    for (size_t i = 0; i < m_knobs.size(); ++i)
    {
        m_knobs[i]->setBounds(knobRow.removeFromLeft(textBoxWidth));
        knobRow.removeFromLeft(i == 2 ? groupGap : gap);
    }
}
