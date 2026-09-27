/**
 * @file EarlyReflectionsPlayground.h
 * @brief Playground for EarlyReflections: the echogram (EchogramView) above one knob
 *        per parameter.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include "../AlgorithmPlayground.h"
#include "EchogramView.h"

class EarlyReflectionsPlayground : public AlgorithmPlayground
{
public:
    EarlyReflectionsPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm);
    void resized() override;

private:
    std::unique_ptr<EchogramView> m_echogram;
    std::vector<std::unique_ptr<PlaygroundKnob>> m_knobs; // in EarlyReflections::ParamIndex order
};
