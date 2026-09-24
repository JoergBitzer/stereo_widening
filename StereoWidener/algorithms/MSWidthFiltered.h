/**
 * @file MSWidthFiltered.h
 * @brief Algorithm 2.1 (planing.md), "bass mono" case: M/S width control with the side
 *        signal high-pass filtered before the width scaling.
 *
 * Same M/S recombination as MSWidthBroadband, but the side signal S is run through a
 * high-pass filter (crossover at kCrossoverHz) before being scaled by width. Content
 * below the crossover is removed from S entirely -- forced into M, i.e. mono -- while
 * content above it gets the normal width control. This is the standard "keep the bass
 * mono" mastering trick: low frequencies translate better to mono playback and carry
 * most of a mix's energy, so collapsing them to the centre is usually inaudible as a
 * width change but avoids phase-cancellation problems on mono sum.
 *
 * Deliberately the second, audibly different algorithm alongside MSWidthBroadband, so
 * switching between the two exercises StereoWidenerAudio's crossfade
 * (see docs/algorithms/phase3_stereo_widener.md).
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include "StereoAlgorithm.h"

class MSWidthFiltered : public StereoAlgorithm
{
public:
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, float width) noexcept override;

    const char* getName() const noexcept override { return "M/S Width (Filtered / Bass Mono)"; }
    bool isMonoSafe() const noexcept override { return true; }
    int getLatencySamples() const noexcept override { return 0; }

    static constexpr float kCrossoverHz = 150.0f;

private:
    juce::dsp::IIR::Filter<float> sideHighpass;
};
