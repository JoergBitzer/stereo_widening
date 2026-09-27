/**
 * @file AllpassPlayground.h
 * @brief Playground for AllpassDecorrelation: what the algorithm does to a centred
 *        (mono) input -- the gain of L, R and the mono sum over frequency -- with the
 *        allpass stages' frequencies marked (L's fixed, R's shifted by Spread), and one
 *        knob per parameter below. Drag left/right for Spread, up/down for Amount.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include "../AlgorithmPlayground.h"
#include "FrequencyGraph.h"

class AllpassPlayground : public AlgorithmPlayground
{
public:
    AllpassPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm);
    void resized() override;

private:
    float gainDb(int channel, float hz) const; // 0 = L, 1 = R, 2 = mono sum
    std::vector<float> stageFrequencies(bool left) const;

    FrequencyGraph m_graph;
    std::vector<std::unique_ptr<PlaygroundKnob>> m_knobs; // in AllpassDecorrelation::ParamIndex order
    AlgorithmParamValues m_values {};
};
