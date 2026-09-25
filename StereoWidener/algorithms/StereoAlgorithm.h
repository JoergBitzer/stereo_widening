/**
 * @file StereoAlgorithm.h
 * @brief Common interface for a stereo-widening algorithm, so StereoWidenerAudio can
 *        switch between algorithms without knowing what each one does internally.
 *
 * One small class per algorithm (see planing.md section 5, "code rules": the codebase
 * is also a teaching example). Each algorithm processes a stereo buffer in place, given
 * the current parameters (see StereoAlgorithmParams below).
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <array>
#include <juce_audio_basics/juce_audio_basics.h>

// Cap for StereoAlgorithmParams::multi / StereoAlgorithm::getNumMultiParams() below --
// see the comment there for why this exists alongside auxLeft/auxRight. 6 is exactly
// what Phase 5's multiband width (algorithm 2.7) needs (3 crossover frequencies + 3
// per-band widths); raise it if a future algorithm needs more.
static constexpr int kMaxMultiParams = 6;

/** Everything an algorithm's process() needs. width is common to every algorithm; the
 *  two aux values are the StereoWidenerGUI's two flanking knobs (left/right of Width) --
 *  what each one actually means (if anything) is entirely up to the active algorithm,
 *  see getAuxLeftInfo()/getAuxRightInfo() below. An algorithm that doesn't use one or
 *  both aux values just ignores them; their knobs are also disabled in the GUI for it
 *  (AuxKnobInfo::enabled = false), so the user never sees a value that does nothing.
 *
 *  `multi` is a separate, larger set for algorithms that need more than two
 *  independent values (currently only multiband width, algorithm 2.7) -- rather than
 *  generalise auxLeft/auxRight themselves (which would force every existing algorithm
 *  and all of StereoWidenerGUI's knob-rebinding code to change for a case only one
 *  algorithm needs), an algorithm just opts in via getNumMultiParams() > 0 and reads
 *  params.multi[0..getNumMultiParams()-1]; every existing algorithm is unaffected and
 *  needs no changes at all (the interface's default implementation returns 0). */
struct StereoAlgorithmParams
{
    float width = 1.0f;    // 0 = mono, 1 = unity/unchanged, 2 = double the side signal
    float auxLeft = 0.0f;
    float auxRight = 0.0f;
    std::array<float, kMaxMultiParams> multi {};
};

/** Describes how the active algorithm wants one of the two aux knobs used, so
 *  StereoWidenerGUI can relabel and enable/disable them per algorithm instead of
 *  showing a knob that silently does nothing. */
struct AuxKnobInfo
{
    bool enabled = false; // false: StereoWidenerGUI greys the knob out for this algorithm
    juce::String label;   // e.g. "Bass Cutoff"; ignored (and normally empty) when !enabled
};

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
     *  have to guard against mono/multichannel input. */
    virtual void process(juce::AudioBuffer<float>& buffer, const StereoAlgorithmParams& params) noexcept = 0;

    /** Display name, shown in the algorithm selector. */
    virtual const char* getName() const noexcept = 0;

    /** Explanation shown in the "?" help popup next to the algorithm selector: what the
     *  algorithm does and why, with a citation to a written source -- this is a teaching
     *  tool as much as a plugin (planing.md section 5). Plain text, may contain '\n'. */
    virtual juce::String getDescription() const = 0;

    /** How this algorithm wants StereoWidenerGUI's left/right aux knobs used. */
    virtual AuxKnobInfo getAuxLeftInfo() const noexcept = 0;
    virtual AuxKnobInfo getAuxRightInfo() const noexcept = 0;

    /** How many of params.multi[] this algorithm uses (0..kMaxMultiParams), and their
     *  labels -- see StereoAlgorithmParams::multi above. StereoWidenerGUI shows a
     *  dedicated grid of knobs (growing the window if needed) only when this is > 0;
     *  0 for every algorithm except multiband width, so the default implementation
     *  here means no other algorithm needs to override either method. */
    virtual int getNumMultiParams() const noexcept { return 0; }
    virtual AuxKnobInfo getMultiParamInfo(int index) const noexcept { juce::ignoreUnused(index); return {}; }

    /** Mastering profile (plan2.md section 2) only offers mono-safe algorithms. */
    virtual bool isMonoSafe() const noexcept = 0;

    /** Extra latency this algorithm adds, in samples, beyond StereoWidenerAudio's own
     *  (currently zero -- see StereoWidener.h). Both algorithms so far report 0; a
     *  future linear-phase crossover would not. */
    virtual int getLatencySamples() const noexcept = 0;
};
