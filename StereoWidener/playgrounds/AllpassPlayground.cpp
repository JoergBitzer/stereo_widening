#include "AllpassPlayground.h"
#include "../PluginSettings.h"
#include "../algorithms/AllpassDecorrelation.h"

AllpassPlayground::AllpassPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm)
    : AlgorithmPlayground(apvts), m_graph(-24.0f, 12.0f)
{
    using AP = AllpassDecorrelation;
    const auto specs = algorithm.getParamSpecs();
    for (const auto& spec : specs)
    {
        m_knobs.push_back(std::make_unique<PlaygroundKnob>(apvts, spec));
        addAndMakeVisible(*m_knobs.back());
    }

    m_graph.addCurve("L", [this](float hz) { return gainDb(0, hz); });
    m_graph.addCurve("R", [this](float hz) { return gainDb(1, hz); });
    m_graph.addCurve("L+R", [this](float hz) { return gainDb(2, hz); });
    m_graph.setReferenceLine(0.0f, "mono input: 0 dB");
    m_graph.addFrequencyMarks([this] { return stageFrequencies(true); }, false, true);
    m_graph.addFrequencyMarks([this] { return stageFrequencies(false); }, true, false);
    m_graph.setFreeDrag(&getParameter(specs[AP::kSpread].id), 100.0f, &getParameter(specs[AP::kAmount].id), 100.0f);
    addAndMakeVisible(m_graph);

    for (size_t i = 0; i < specs.size(); ++i)
        watchParameter(specs[i].id, [this, i](float value)
        {
            m_values[i] = value;
            m_graph.repaint();
        });
}

float AllpassPlayground::gainDb(int channel, float hz) const
{
    float db[3] {};
    AllpassDecorrelation::monoInputGainDb(hz, m_values, kDisplaySampleRate, db[0], db[1], db[2]);
    return db[channel];
}

std::vector<float> AllpassPlayground::stageFrequencies(bool left) const
{
    const float spreadOctaves = m_values[AllpassDecorrelation::kSpread] * 0.01f * AllpassDecorrelation::kMaxSpreadOctaves;
    std::vector<float> frequencies;
    for (int stage = 0; stage < AllpassDecorrelation::kNumStages; ++stage)
        frequencies.push_back(AllpassDecorrelation::stageFrequencyHz(stage, left, spreadOctaves, kDisplaySampleRate));
    return frequencies;
}

void AllpassPlayground::resized()
{
    const int textBoxWidth = scaled(g_smallKnobTextBoxWidth);
    for (auto& knob : m_knobs)
        knob->setKnobLayout(scaled(g_smallKnobSize), scaled(g_smallKnobLabelHeight), textBoxWidth);
    m_graph.setScaleFactor(m_scale);

    // Graph across the full width on top, the knobs in one row below.
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
