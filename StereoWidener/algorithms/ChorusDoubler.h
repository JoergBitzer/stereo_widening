/**
 * @file ChorusDoubler.h
 * @brief Algorithm 2.11 (planing.md): micro-pitch / chorus doubler ("Dimension D"
 *        style).
 *
 * planing.md 2.11: "L and R get slightly different short, modulated delays (5-30 ms,
 * LFO) ... the classic 'Dimension D' or 'micro shift' effect (Eventide H3000 style)."
 * Group S+P (works on existing stereo content and creates pseudo-stereo from mono),
 * mono-compatibility "-" -- worse than algorithm 2.5's allpass decorrelation or 2.12's
 * early reflections (both "o"), matching planing.md's own con: "mono sum shows comb
 * and flanging artefacts". Per explicit user decision, this implements only the
 * modulated-delay chorus/Dimension-D member of planing.md 2.11's family, not the
 * fixed-cent micro-pitch-shift variant (needs a structurally different ramp/sawtooth
 * LFO with a crossfading delay line -- a real redesign, out of scope here).
 *
 * The mid signal M is fed through two independently LFO-modulated delay lines, one per
 * output channel, with the two LFOs held a constant quarter-cycle (90 deg,
 * "quadrature") apart -- the actual source of L/R decorrelation, same reason algorithm
 * 2.5 uses two different cascades and 2.12 uses two different tap patterns:
 *
 *     delayL(t) = kBaseDelayMs + Depth*kMaxDepthMs*sin(2*pi*rate*t)
 *     delayR(t) = kBaseDelayMs + kStereoOffsetMs
 *                 + Depth*kMaxDepthMs*sin(2*pi*rate*t + kStereoPhaseOffsetRadians)
 *     Y_L[n] = M[n - delayL(n)] (fractional/interpolated read) - M[n]
 *     Y_R[n] = M[n - delayR(n)] (fractional/interpolated read) - M[n]
 *     L' = L + Width*Amount*Y_L
 *     R' = R + Width*Amount*Y_R
 *
 * Same structural pattern as 2.5/2.12 (add directly to L/R) rather than 2.4's comb
 * (S' = S + gain*...): two different, continuously time-varying delay patterns added
 * to L and R means L'+R' generally does NOT equal L+R once Amount > 0 -- the "-"
 * mono-compatibility rating (worse than 2.5/2.12's "o": here the interference pattern
 * between L and R is itself constantly sweeping with the LFO, "flanging" through a
 * range of comb-filter notch positions over time, rather than sitting at one fixed
 * set of notches). isMonoSafe() reports false; StereoWidenerGUI shows the "not
 * mono-safe" badge for this algorithm, same as AllpassDecorrelation/EarlyReflections.
 *
 * kStereoOffsetMs is a FIXED L/R base separation, deliberately NOT scaled by Depth: an
 * earlier version of the Python reference (python/algorithms/chorus_doubler.py) scaled
 * the entire LFO excursion -- including the base phase-offset term -- by Depth, so
 * Depth=0 collapsed delayL and delayR to the exact same constant, i.e. L and R read
 * the IDENTICAL delayed copy of M. That is not "no chorus", it is a single comb filter
 * baked identically into both channels (no complementary +/- structure like
 * ComplementaryComb, so each channel's own spectrum AND the mono sum both take the
 * full notch pattern) -- found via evaluate_chorus_doubler.py showing Depth=0 as the
 * *worst* setting for dLUFS/mono coloration, backwards from what "turn depth down"
 * should do. Fixed by keeping a small constant L/R separation independent of Depth, so
 * the two channels always stay genuinely different, matching planing.md's own
 * description ("L and R get slightly different ... delays") as unconditional.
 *
 * Uses TWO SEPARATE juce::dsp::DelayLine instances (one per channel), each read with
 * plain one-push/one-pop-per-sample usage (default updateReadPointer=true) --
 * deliberately NOT the shared-single-delay-line-with-multiple-taps trick
 * EarlyReflections uses, even though M is the same signal for both channels. That
 * trick requires getting popSample()'s updateReadPointer exactly right for every tap
 * (see EarlyReflections.cpp's own account of two read-cursor bugs found there); with
 * only two taps the memory saved is negligible, so the simpler, provably-correct
 * one-line-per-channel design is used instead.
 *
 * Depth's value is smoothed (juce::SmoothedValue, same fix as ComplementaryComb's own
 * "zipper noise on Delay changes" -- see phase5_comb.md): it directly scales the LFO
 * excursion fed into setDelay() every sample, so an abrupt Depth change would
 * otherwise step the delay line's target position discontinuously. Amount is a plain
 * output-gain multiplier, matching AllpassDecorrelation/EarlyReflections' precedent of
 * not smoothing that kind of parameter. Rate is a GlobalSettings default, not smoothed
 * (not user-adjustable during play, like ComplementaryComb's crossoverHz).
 *
 * Two user-facing parameters, per the project's "2 + Width" convention:
 * - Amount (StereoWidenerGUI's left aux knob): 0-100 %, defaults to 0 % -- an exact,
 *   algebraically neutral bypass (see phase5_comb.md's "Removing last-used state").
 * - Depth (right aux knob): 0-100 %, scales the LFO's modulation excursion. No neutral
 *   value of its own -- inert whenever Amount = 0, same reasoning as comb's Delay/
 *   allpass's Spread/early reflections' Room Size defaults.
 * - Rate (Hz): NOT a user-facing parameter -- a GlobalSettings default (like comb's
 *   crossover frequency, early reflections' pre-delay), kept deliberately slow/
 *   "Dimension D"-like by design: a fast rate turns this into an obvious vibrato/
 *   warble, a different, arguably worse-sounding effect for a width tool. Exposing it
 *   live risks users dialling in that worse-sounding regime, so it is fixed rather
 *   than a third knob. See setRateHz().
 *
 * Reference: the "Dimension D" / stereo chorus family described in planing.md 2.11;
 * the underlying "modulated delay line" chorus technique is standard (e.g. Dattorro,
 * "Effect Design Part 2: Delay Line Modulation and Chorus", JAES 1997).
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include "StereoAlgorithm.h"

class ChorusDoubler : public StereoAlgorithm
{
public:
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, const StereoAlgorithmParams& params) noexcept override;

    const char* getName() const noexcept override { return "Chorus Doubler (Dimension D)"; }
    juce::String getDescription() const override;
    AuxKnobInfo getAuxLeftInfo() const noexcept override { return { true, "Amount" }; }
    AuxKnobInfo getAuxRightInfo() const noexcept override { return { true, "Depth" }; }
    bool isMonoSafe() const noexcept override { return false; }
    int getLatencySamples() const noexcept override { return 0; }

    /** User-configurable default (GlobalSettings, Phase 4): the LFO rate in Hz. Not
     *  itself a user-facing parameter -- see the file header for why. */
    void setRateHz(float hz) noexcept { rateHz = hz; }

    static constexpr float kBaseDelayMs = 15.0f;
    static constexpr float kStereoOffsetMs = 3.0f;
    static constexpr float kMaxDepthMs = 10.0f;
    static constexpr float kStereoPhaseOffsetRadians = juce::MathConstants<float>::halfPi;
    static constexpr float kMaxDelayMs = kBaseDelayMs + kStereoOffsetMs + kMaxDepthMs;
    static constexpr float kDepthSmoothingSeconds = 0.05f;

private:
    double sampleRate = 48000.0;
    float rateHz = 0.3f;
    float phase = 0.0f; // radians, wrapped to [0, 2*pi) every sample -- see process()

    juce::SmoothedValue<float> smoothedDepth;
    bool depthInitialized = false;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL { 4096 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR { 4096 };
};
