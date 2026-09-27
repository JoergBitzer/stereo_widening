#include "AlgorithmPlayground.h"
#include "PluginSettings.h"
#include "algorithms/MultibandWidth.h"
#include "playgrounds/MultibandPlayground.h"
#include "algorithms/MSWidthFiltered.h"
#include "playgrounds/FilteredPlayground.h"

PlaygroundKnob::PlaygroundKnob(juce::AudioProcessorValueTreeState& apvts, const AlgorithmParamSpec& spec, const juce::String& label)
{
    m_label.setText(label.isNotEmpty() ? label : juce::String(spec.name), juce::dontSendNotification);
    m_label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_label);
    addAndMakeVisible(m_slider);
    m_attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, spec.id, m_slider);
}

void PlaygroundKnob::setKnobLayout(int knobSize, int textHeight, int textBoxWidth)
{
    m_knobSize = knobSize;
    m_textHeight = textHeight;
    m_textBoxWidth = textBoxWidth;
    m_slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, m_textBoxWidth, m_textHeight);
    resized();
}

void PlaygroundKnob::resized()
{
    auto r = getLocalBounds();
    m_label.setBounds(r.removeFromTop(m_textHeight));
    m_slider.setBounds(r.removeFromTop(m_knobSize + m_textHeight));
}

KnobsPlayground::KnobsPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm)
    : AlgorithmPlayground(apvts)
{
    for (const auto& spec : algorithm.getParamSpecs())
    {
        m_knobs.push_back(std::make_unique<PlaygroundKnob>(apvts, spec));
        addAndMakeVisible(*m_knobs.back());
    }
}

void KnobsPlayground::resized()
{
    jassert(m_knobs.size() <= 3); // more parameters need a dedicated playground
    getKnob(0).setKnobLayout(scaled(g_widthKnobSize), scaled(g_widthKnobLabelHeight), scaled(g_widthKnobSize));
    for (size_t i = 1; i < m_knobs.size(); ++i)
        m_knobs[i]->setKnobLayout(scaled(g_auxKnobSize), scaled(g_auxKnobLabelHeight), scaled(g_auxKnobSize));

    // left-to-right order: second parameter, main knob, third parameter
    std::vector<PlaygroundKnob*> row;
    if (m_knobs.size() >= 2)
        row.push_back(m_knobs[1].get());
    row.push_back(m_knobs[0].get());
    if (m_knobs.size() >= 3)
        row.push_back(m_knobs[2].get());

    const int gap = scaled(g_paramKnobGap);
    int totalWidth = gap * ((int) row.size() - 1);
    for (auto* knob : row)
        totalWidth += knob->getPreferredWidth();

    auto area = getLocalBounds().withSizeKeepingCentre(totalWidth, getHeight());
    for (auto* knob : row)
    {
        auto column = area.removeFromLeft(knob->getPreferredWidth());
        area.removeFromLeft(gap);
        knob->setBounds(column.withSizeKeepingCentre(column.getWidth(), knob->getPreferredHeight()));
    }
}

std::unique_ptr<AlgorithmPlayground> createPlayground(juce::AudioProcessorValueTreeState& apvts,
                                                      const StereoAlgorithm& algorithm)
{
    if (dynamic_cast<const MSWidthFiltered*>(&algorithm) != nullptr)
        return std::make_unique<FilteredPlayground>(apvts, algorithm);
    if (dynamic_cast<const MultibandWidth*>(&algorithm) != nullptr)
        return std::make_unique<MultibandPlayground>(apvts, algorithm);
    return std::make_unique<KnobsPlayground>(apvts, algorithm);
}
