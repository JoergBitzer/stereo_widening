#include "AlgorithmPlayground.h"
#include "PluginSettings.h"
#include "algorithms/MultibandWidth.h"
#include "algorithms/MSWidthBroadband.h"
#include "playgrounds/BroadbandPlayground.h"

PlaygroundKnob::PlaygroundKnob(juce::AudioProcessorValueTreeState& apvts, const AlgorithmParamSpec& spec)
{
    m_label.setText(spec.name, juce::dontSendNotification);
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
    if (m_knobs.size() <= 3)
        layoutAroundMainKnob();
    else
        layoutGrid();
}

void KnobsPlayground::layoutAroundMainKnob()
{
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

void KnobsPlayground::layoutGrid()
{
    const int textBoxWidth = scaled(g_gridKnobTextBoxWidth);
    for (auto& knob : m_knobs)
        knob->setKnobLayout(scaled(g_gridKnobSize), scaled(g_gridKnobLabelHeight), textBoxWidth);

    const int numKnobs = (int) m_knobs.size();
    const int rows = (numKnobs + g_gridMaxColumns - 1) / g_gridMaxColumns;
    const int colGap = scaled(g_gridKnobColGap);
    const int rowGap = scaled(g_gridKnobRowGap);
    const int cellHeight = m_knobs.front()->getPreferredHeight();

    auto area = getLocalBounds().withSizeKeepingCentre(getWidth(), rows * cellHeight + (rows - 1) * rowGap);
    for (int row = 0; row < rows; ++row)
    {
        const int first = row * g_gridMaxColumns;
        const int count = juce::jmin(g_gridMaxColumns, numKnobs - first);
        auto rowArea = area.removeFromTop(cellHeight);
        area.removeFromTop(rowGap);
        rowArea = rowArea.withSizeKeepingCentre(count * textBoxWidth + (count - 1) * colGap, cellHeight);
        for (int i = first; i < first + count; ++i)
        {
            getKnob(i).setBounds(rowArea.removeFromLeft(textBoxWidth));
            rowArea.removeFromLeft(colGap);
        }
    }
}

namespace
{
    /** KnobsPlayground plus MultibandWidth's crossover ordering: a crossover knob stops
     *  at its neighbours instead of passing them, so "which knob is the low-mid split"
     *  never silently swaps. The DSP sorts defensively too (a host can automate the
     *  parameters directly), see MultibandWidth::updateFrequenciesIfNeeded(). */
    class MultibandKnobsPlayground : public KnobsPlayground
    {
    public:
        MultibandKnobsPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm)
            : KnobsPlayground(apvts, algorithm)
        {
            // Wired only after every knob's attachment has synced its initial value, so
            // no crossover is ever compared against a neighbour that isn't set yet.
            for (int index : { MultibandWidth::kFreq1, MultibandWidth::kFreq2, MultibandWidth::kFreq3 })
                getKnob(index).getSlider().onValueChange = [this, index] { keepCrossoverBetweenNeighbours(index); };
        }

    private:
        void keepCrossoverBetweenNeighbours(int index)
        {
            constexpr double minRatio = 1.05; // adjacent crossovers stay >= 5 % apart, matches the DSP's own safety net
            auto& slider = getKnob(index).getSlider();
            if (index > MultibandWidth::kFreq1)
            {
                const double lower = getKnob(index - 1).getSlider().getValue() * minRatio;
                if (slider.getValue() < lower)
                    slider.setValue(lower, juce::sendNotificationSync);
            }
            if (index < MultibandWidth::kFreq3)
            {
                const double upper = getKnob(index + 1).getSlider().getValue() / minRatio;
                if (slider.getValue() > upper)
                    slider.setValue(upper, juce::sendNotificationSync);
            }
        }
    };
}

std::unique_ptr<AlgorithmPlayground> createPlayground(juce::AudioProcessorValueTreeState& apvts,
                                                      const StereoAlgorithm& algorithm)
{
    if (dynamic_cast<const MSWidthBroadband*>(&algorithm) != nullptr)
        return std::make_unique<BroadbandPlayground>(apvts, algorithm);
    if (dynamic_cast<const MultibandWidth*>(&algorithm) != nullptr)
        return std::make_unique<MultibandKnobsPlayground>(apvts, algorithm);
    return std::make_unique<KnobsPlayground>(apvts, algorithm);
}
