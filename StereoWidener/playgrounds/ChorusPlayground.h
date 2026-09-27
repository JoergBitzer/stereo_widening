/**
 * @file ChorusPlayground.h
 * @brief Playground for ChorusDoubler: the two channels' modulated delay times
 *        (DelayModulationView) above one knob per parameter.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include "../AlgorithmPlayground.h"
#include "DelayModulationView.h"

class ChorusPlayground : public AlgorithmPlayground
{
public:
    ChorusPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm);
    void resized() override;

private:
    std::unique_ptr<DelayModulationView> m_view;
    std::vector<std::unique_ptr<PlaygroundKnob>> m_knobs; // in ChorusDoubler::ParamIndex order
};
