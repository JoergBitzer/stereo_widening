/**
 * @file MSWidthBroadband.h
 * @brief Algorithm 2.1 (planing.md), broadband case: plain M/S width control.
 *
 * M = (L+R)/2, S = (L-R)/2 (same notation as StereoMeterState and python/stereo_eval).
 * Scaling S by width and recombining (L' = M + width*S, R' = M - width*S) is the
 * textbook stereo-width control: width = 0 collapses to mono, width = 1 is the
 * identity (bit-exact passthrough, used for the null test in
 * docs/algorithms/phase3_stereo_widener.md), width = 2 doubles the side signal.
 *
 * No filtering: the whole spectrum (including the bass) is widened equally. See
 * MSWidthFiltered.h for the "bass mono" alternative that keeps low frequencies
 * centred, which exists specifically to exercise the algorithm-switch crossfade
 * (StereoWidenerAudio::processSynchronBlock) against a second, audibly different mode.
 * Stateless (no filters, no memory), so prepare()/reset() have nothing to do.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include "StereoAlgorithm.h"

class MSWidthBroadband : public StereoAlgorithm
{
public:
    void prepare(double sampleRate, int maxBlockSize) override { juce::ignoreUnused(sampleRate, maxBlockSize); }
    void reset() override {}
    void process(juce::AudioBuffer<float>& buffer, float width) noexcept override;

    const char* getName() const noexcept override { return "M/S Width (Broadband)"; }
    bool isMonoSafe() const noexcept override { return true; }
    int getLatencySamples() const noexcept override { return 0; }
};
