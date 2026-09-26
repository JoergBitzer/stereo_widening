/**
 * @file UtilityProcessor.h
 * @brief Stereo-image utilities (planing.md 2.13, "not algorithms, but should be in
 *        the plugin") plus stereo rotation (2.2) -- applied once, after the currently
 *        selected width algorithm's (possibly crossfaded) output, regardless of which
 *        one is active. "Together with M/S width this gives a complete 'image editor'"
 *        (planing.md 2.2).
 *
 * Processing order: Rotation (the 2x2 image-plane matrix, 2.2) -> Balance (relative
 * L/R level) -> polarity Invert L/R -> Swap L/R -> Monitor mode -> output Gain trim.
 * Monitor mode is an override for auditioning: it replaces L/R with either the mono
 * sum or the side signal, always reflecting everything upstream. This one control
 * covers two entries from planing.md 2.13 that are the same DSP operation viewed two
 * ways -- "Mono (sum)" and "mono check (listen to L+R)" -- plus "solo side (listen to
 * S)"; L/R swap and polarity invert are the other two entries there, verbatim. Gain
 * is applied last of all, deliberately after Monitor mode, so it also trims whatever
 * is currently being auditioned, the same way a final output fader would.
 *
 * Stateless (pure per-sample math, no filters or memory), so there is no need for
 * prepare()/reset() -- unlike the StereoAlgorithm classes in algorithms/, this always
 * runs regardless of which width algorithm is selected, so it is not itself a
 * StereoAlgorithm.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

struct UtilityParams
{
    float rotationDeg = 0.0f; // -45..+45 (planing.md 2.2); positive rotates towards R
    float balance = 0.0f;     // -1..+1; negative attenuates R (towards L), positive attenuates L (towards R)
    bool invertL = false;
    bool invertR = false;
    bool swapLR = false;

    enum MonitorMode { Normal = 0, MonoCheck = 1, SoloSide = 2 };
    int monitorMode = Normal;

    float gainDb = 0.0f; // -24..+6, applied last (see the processing-order comment above)
};

class UtilityProcessor
{
public:
    void process(juce::AudioBuffer<float>& buffer, const UtilityParams& params) const noexcept;
};
