/**
 * @file StereoAlgorithm.h
 * @brief Common interface for a stereo-widening algorithm, so StereoWidenerAudio can
 *        switch between algorithms without knowing what each one does internally.
 *
 * One small class per algorithm (see planing.md section 5, "code rules": the codebase
 * is also a teaching example). Each algorithm processes a stereo buffer in place, given
 * the current Width parameter (0 = mono, 1 = unity/unchanged, 2 = double side signal).
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

class StereoAlgorithm
{
public:
    virtual ~StereoAlgorithm() = default;

    /** Call from prepareToPlay before the first process() call, and again whenever the
     *  sample rate or block size changes. */
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;

    /** Clears any internal filter/delay state. Called when this algorithm becomes the
     *  active one again after being switched away from, so stale state from before the
     *  switch never leaks into new audio. */
    virtual void reset() = 0;

    /** Processes buffer in place. buffer always has exactly 2 channels (L, R); the
     *  caller (StereoWidenerAudio) is responsible for that, so algorithms don't each
     *  have to guard against mono/multichannel input. width: 0 = mono (no side signal),
     *  1 = unity (unchanged from the input), 2 = double the side signal. */
    virtual void process(juce::AudioBuffer<float>& buffer, float width) noexcept = 0;

    /** Display name, shown in the algorithm selector. */
    virtual const char* getName() const noexcept = 0;

    /** Mastering profile (plan2.md section 2) only offers mono-safe algorithms. */
    virtual bool isMonoSafe() const noexcept = 0;

    /** Extra latency this algorithm adds, in samples, beyond StereoWidenerAudio's own
     *  (currently zero -- see StereoWidener.h). Both algorithms so far report 0; a
     *  future linear-phase crossover would not. */
    virtual int getLatencySamples() const noexcept = 0;
};
