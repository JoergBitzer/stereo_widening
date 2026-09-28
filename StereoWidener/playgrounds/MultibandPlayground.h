/**
 * @file MultibandPlayground.h
 * @brief Playground for MultibandWidth: the band-split element (BandSplitView) above a
 *        two staggered rows of compact knobs for exact values: the three splits
 *        (crossovers) on top, the widths of bands 2-4 below, each between the splits
 *        that bound its band. No overall Width: each band has its own.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include "../AlgorithmPlayground.h"
#include "BandSplitView.h"

class MultibandPlayground : public AlgorithmPlayground
{
public:
    MultibandPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm);
    void resized() override;

private:
    void limitCrossoverKnobs();

    std::unique_ptr<BandSplitView> m_bandSplit;
    std::vector<std::unique_ptr<PlaygroundKnob>> m_knobs; // in MultibandWidth::ParamIndex order
};
