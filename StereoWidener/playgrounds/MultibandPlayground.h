/**
 * @file MultibandPlayground.h
 * @brief Playground for MultibandWidth: the band-split element (BandSplitView) above a
 *        row of compact knobs for exact values, in two captioned groups -- Frequency
 *        (the three splits/crossovers) and Width (bands 2-4). No overall Width: each
 *        band has its own.
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
    juce::Label m_frequencyCaption;
    juce::Label m_widthCaption;
    std::vector<std::unique_ptr<PlaygroundKnob>> m_knobs; // in MultibandWidth::ParamIndex order
};
