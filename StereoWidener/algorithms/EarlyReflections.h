/**
 * @file EarlyReflections.h
 * @brief Algorithm 2.12 (planing.md): early-reflection / room widening.
 *
 * planing.md 2.12: "Add a few short, *decorrelated* early reflections (different for L
 * and R, within 5-40 ms, low level). The effect is apparent source width (ASW), known
 * from room acoustics." Group S+P (works on existing stereo content and creates
 * pseudo-stereo from mono), mono-compatibility "o" (partial, like algorithm 2.5's
 * allpass decorrelation).
 *
 * kNumReflections delayed, decreasing-gain copies of the mid signal M are added to
 * each channel, using a DIFFERENT set of delay times per channel (the actual source of
 * decorrelation/width -- "different for L and R"), matching algorithm 2.5's own
 * structural pattern (add to L/R directly, not just to S) rather than algorithm 2.4's
 * comb (S' = S + gain*...):
 *
 *     Y_L = sum_k gain[k] * M[n - delayL[k]]
 *     Y_R = sum_k gain[k] * M[n - delayR[k]]
 *     L1 = L + Amount * Y_L,   R1 = R + Amount * Y_R
 *     M' = (L1 + R1) / 2,      S' = Width * (L1 - R1) / 2
 *     L' = M' + S',            R' = M' - S'
 *
 * Width is applied last, as a plain M/S width on the result -- the same as every
 * other algorithm (0 % = mono output, 100 % = unchanged, 200 % = extra wide), and the
 * same pattern as AllpassDecorrelation. Until v0.1.26 Width instead multiplied the
 * added reflections (L' = L + Width * Amount * Y_L), i.e. it was just a second Amount;
 * at Width 100 % both give the same output.
 *
 * Adding a DIFFERENT reflection pattern to L than to R means L'+R' generally does NOT
 * equal L+R once Amount > 0 -- planing.md's own "o" (partial) mono-compatibility
 * rating, the same trade-off algorithm 2.5 makes for the same underlying reason (a
 * technique that decorrelates channels by construction cannot also leave the mono sum
 * untouched). isMonoSafe() reports false; StereoWidenerGUI shows the "not mono-safe"
 * badge for this algorithm, same as AllpassDecorrelation.
 *
 * All kNumReflections*2 taps read from ONE shared delay line (fed with M each sample)
 * at different, independently smoothed delay offsets -- see .cpp -- rather than one
 * delay line per tap, since they all delay the same signal (M) and JUCE's DelayLine
 * supports re-reading a pushed sample at any offset via repeated setDelay()+
 * popSample() calls before the next pushSample(). Exactly ONE of those
 * kNumReflections*2 popSample() calls per audio sample (the temporally last one) is
 * passed updateReadPointer=true, the rest false: that flag controls DelayLine's OWN
 * internal read cursor, which must advance by exactly one step per pushed sample to
 * stay locked to pushSample()'s write cursor (as it implicitly does in the usual
 * one-push/one-pop-per-sample pattern, e.g. ComplementaryComb's single tap). Passing
 * true on every call over-advances it (each pop nudges it further, corrupting every
 * tap's actual delay within a few dozen samples); passing false on every call never
 * advances it at all (every tap then reads a single fixed buffer slot instead of "N
 * samples ago", refreshed only once per buffer revolution -- audible as periodic
 * crackle, and NOT visible in this project's usual statistical evaluation metrics,
 * since the stale, rarely-refreshed samples read back are still real, correlated
 * audio -- see .cpp for the full account of both mistakes and the fix). Each tap's
 * delay value is smoothed
 * (juce::SmoothedValue, same fix as ComplementaryComb's own "zipper noise on Delay
 * changes" -- see phase5_comb.md), since Room Size changing live would otherwise step
 * every tap's read position discontinuously.
 *
 * Parameters (getParamSpecs()):
 * - Width: M/S width of the output, like every algorithm's Width (see above).
 * - Amount: 0-100 %, defaults to 0 % -- an exact, algebraically neutral bypass.
 * - Room Size: 0-100 %, no neutral value of its own -- inert whenever Amount = 0.
 *   Scales the spread of the reflection pattern from a small room (tight, early
 *   cluster) to a large room (wider spread, further into planing.md's suggested
 *   5-40 ms window).
 * - Pre-delay: 0-20 ms, default 5 ms -- the time before the first reflection. (Until
 *   v0.1.23 a fixed value from the global settings file, not a parameter.)
 * The number of reflections per channel is a compiled-in constant (kNumReflections),
 * analogous to AllpassDecorrelation's fixed cascade-stage count.
 *
 * Reference: apparent source width (ASW) via early lateral reflections is standard
 * room-acoustics/concert-hall literature; see e.g. L. Beranek, "Concert Halls and Opera
 * Houses: Music, Acoustics, and Architecture", 2nd ed., Springer, 2004, ch. 2.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <array>
#include <juce_dsp/juce_dsp.h>
#include "StereoAlgorithm.h"

class EarlyReflections : public StereoAlgorithm
{
public:
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept override;

    enum ParamIndex { kWidth = 0, kAmount, kRoomSize, kPreDelay };
    std::vector<AlgorithmParamSpec> getParamSpecs() const override
    {
        return {
            AlgorithmParamSpec::width("earlyReflWidth"),
            AlgorithmParamSpec::linear("earlyReflAmount", "Amount", "%", 0.0f, 100.0f, 0.0f)
                .withHelp("level of the reflections. 0 % = no effect."),
            AlgorithmParamSpec::linear("earlyReflRoomSize", "Room Size", "%", 0.0f, 100.0f, 50.0f)
                .withHelp("how spread out the reflections are: small = a tight cluster, large = spread over up to 32 ms."),
            AlgorithmParamSpec::linear("earlyReflPreDelay", "Pre-delay", "ms", 0.0f, kMaxPreDelayMs, 5.0f, 1)
                .withHelp("time between the direct sound and the first reflection.")
        };
    }

    // Pure math for the GUI's display (no GUI dependency), the same formulas
    // process() uses.

    /** Time of reflection k (0..kNumReflections-1) of one channel, in ms after the
     *  direct sound. roomSize is 0..1. */
    static float tapTimeMs(int k, bool left, float roomSize, float preDelayMs) noexcept
    {
        const float spreadMs = kRoomMinSpreadMs + roomSize * (kRoomMaxSpreadMs - kRoomMinSpreadMs);
        return preDelayMs + (left ? kLeftFractions : kRightFractions)[(size_t) k] * spreadMs;
    }

    /** Level of reflection k relative to the direct sound, before Amount. */
    static float tapGain(int k) noexcept { return kBaseGain * std::pow(kGainDecay, (float) k); }

    const char* getName() const noexcept override { return "Early Reflections (Room Widening)"; }
    juce::String getDescription() const override;
    bool isMonoSafe() const noexcept override { return false; }
    int getLatencySamples() const noexcept override { return 0; }

    static constexpr int kNumReflections = 5;
    // Fractional tap positions within the room-size spread window (0..1), irregular
    // and DIFFERENT per channel -- the actual source of L/R decorrelation, matching
    // python/algorithms/early_reflections.py's _L_FRACTIONS/_R_FRACTIONS exactly.
    static constexpr std::array<float, kNumReflections> kLeftFractions { 0.06f, 0.24f, 0.43f, 0.66f, 0.90f };
    static constexpr std::array<float, kNumReflections> kRightFractions { 0.11f, 0.30f, 0.52f, 0.74f, 0.97f };
    static constexpr float kBaseGain = 0.45f;
    static constexpr float kGainDecay = 0.7f;
    // Room Size (0-100 %) maps to a spread window this wide, in ms -- matches the
    // Python reference's _ROOM_MIN_SPREAD_MS/_ROOM_MAX_SPREAD_MS.
    static constexpr float kRoomMinSpreadMs = 8.0f;
    static constexpr float kRoomMaxSpreadMs = 32.0f;
    // Pre-delay's range maximum; plus the largest possible spread, the shared delay
    // line never needs to grow after prepare().
    static constexpr float kMaxPreDelayMs = 20.0f;
    static constexpr float kMaxDelayMs = kMaxPreDelayMs + kRoomMaxSpreadMs;
    // Matches ComplementaryComb::kDelaySmoothingSeconds -- same "avoid zipper noise on
    // a live delay-time change" fix, see the file header.
    static constexpr float kDelaySmoothingSeconds = 0.02f;

private:
    void updateTapTargets(float roomSize, float preDelayMs) noexcept;

    double sampleRate = 48000.0;
    float lastRoomSize = -1.0f;
    float lastPreDelayMs = -1.0f;
    bool delayInitialized = false;

    std::array<float, kNumReflections> gains {};

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> midDelayLine { 4096 };
    std::array<juce::SmoothedValue<float>, kNumReflections> leftSmoothedDelaySamples;
    std::array<juce::SmoothedValue<float>, kNumReflections> rightSmoothedDelaySamples;
};
