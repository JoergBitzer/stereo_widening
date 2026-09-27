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
 *     L1 = L + Amount*Y_L,   R1 = R + Amount*Y_R
 *     M' = (L1 + R1)/2,      S' = Width*(L1 - R1)/2
 *     L' = M' + S',          R' = M' - S'
 *
 * Width is applied last, as a plain M/S width on the result -- the same as every
 * other algorithm (0 % = mono output, 100 % = unchanged, 200 % = extra wide), and the
 * same pattern as AllpassDecorrelation. Until v0.1.26 Width instead multiplied the
 * added effect (L' = L + Width*Amount*Y_L), i.e. it was just a second Amount; at
 * Width 100 % both give the same output.
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
 * not smoothing that kind of parameter. Rate needs no smoothing: it only sets the phase
 * increment, the phase itself stays continuous.
 *
 * Parameters (getParamSpecs()):
 * - Width: M/S width of the output, like every algorithm's Width (see above).
 * - Amount: 0-100 %, defaults to 0 % -- an exact, algebraically neutral bypass.
 * - Depth: 0-100 %, scales the LFO's modulation excursion. No neutral value of its
 *   own -- inert whenever Amount = 0.
 * - Rate: 0.05-2 Hz, default 0.3 Hz. (Until v0.1.24 a fixed value from the global
 *   settings file.) Deliberately capped at 2 Hz: the slow, "Dimension D"-like range is
 *   the point; faster rates turn this into an obvious vibrato/warble, a different,
 *   arguably worse-sounding effect for a width tool.
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
    void process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept override;

    enum ParamIndex { kWidth = 0, kAmount, kDepth, kRate };
    std::vector<AlgorithmParamSpec> getParamSpecs() const override
    {
        return {
            AlgorithmParamSpec::width("chorusWidth"),
            AlgorithmParamSpec::linear("chorusAmount", "Amount", "%", 0.0f, 100.0f, 0.0f),
            AlgorithmParamSpec::linear("chorusDepth", "Depth", "%", 0.0f, 100.0f, 50.0f),
            AlgorithmParamSpec::linear("chorusRate", "Rate", "Hz", 0.05f, 2.0f, 0.3f, 2)
        };
    }

    /** Delay time of one channel's modulated delay line in ms, at LFO phase
     *  phaseRadians; depth is 0..1. The formula process() uses (there in samples).
     *  Pure math for the GUI's display. */
    static float delayMs(bool left, float depth, float phaseRadians) noexcept
    {
        return left ? kBaseDelayMs + depth * kMaxDepthMs * std::sin(phaseRadians)
                    : kBaseDelayMs + kStereoOffsetMs + depth * kMaxDepthMs * std::sin(phaseRadians + kStereoPhaseOffsetRadians);
    }

    const char* getName() const noexcept override { return "Chorus Doubler (Dimension D)"; }
    juce::String getDescription() const override;
    bool isMonoSafe() const noexcept override { return false; }
    int getLatencySamples() const noexcept override { return 0; }

    static constexpr float kBaseDelayMs = 15.0f;
    static constexpr float kStereoOffsetMs = 3.0f;
    static constexpr float kMaxDepthMs = 10.0f;
    static constexpr float kStereoPhaseOffsetRadians = juce::MathConstants<float>::halfPi;
    static constexpr float kMaxDelayMs = kBaseDelayMs + kStereoOffsetMs + kMaxDepthMs;
    static constexpr float kDepthSmoothingSeconds = 0.05f;

private:
    double sampleRate = 48000.0;
    float phase = 0.0f; // radians, wrapped to [0, 2*pi) every sample -- see process()

    juce::SmoothedValue<float> smoothedDepth;
    bool depthInitialized = false;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL { 4096 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR { 4096 };
};
