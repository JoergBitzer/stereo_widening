/**
 * @file CombPlayground.h
 * @brief Playground for ComplementaryComb: what the algorithm does to a centred (mono)
 *        input -- the complementary comb responses of L and R (peaks in one where the
 *        other has notches) on a FrequencyGraph with a linear 0-2 kHz axis, the
 *        crossover as a draggable marker, and one knob per parameter below.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include "../AlgorithmPlayground.h"
#include "FrequencyGraph.h"

class CombPlayground : public AlgorithmPlayground
{
public:
    CombPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm);
    void resized() override;

private:
    juce::Range<float> channelGainDbRange(bool left, float loHz, float hiHz) const;

    FrequencyGraph m_graph;
    std::vector<std::unique_ptr<PlaygroundKnob>> m_knobs; // in ComplementaryComb::ParamIndex order
    AlgorithmParamValues m_values {};
};
