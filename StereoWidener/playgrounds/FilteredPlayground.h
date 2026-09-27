/**
 * @file FilteredPlayground.h
 * @brief Playground for MSWidthFiltered: the side signal's gain over frequency (Width,
 *        bass-mono high-pass and air shelf combined) on a FrequencyGraph, with the
 *        cutoff and shelf as draggable points, and one knob per parameter below.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include "../AlgorithmPlayground.h"
#include "FrequencyGraph.h"

class FilteredPlayground : public AlgorithmPlayground
{
public:
    FilteredPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm);
    void resized() override;

private:
    FrequencyGraph m_graph;
    std::vector<std::unique_ptr<PlaygroundKnob>> m_knobs; // in MSWidthFiltered::ParamIndex order
    AlgorithmParamValues m_values {};
};
